/*
 * chat-server.c
 *
 * Updated to parse a custom protocol:
 *   CLIENTIPADDRESS|CLIENTUSERNAME|MESSAGECOUNT|"Message text"
 *
 * The server then broadcasts the message in the format:
 *   IPADDRESS [USERNAME] "MESSAGE"
 * to ALL connected clients (including the sender).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h> // for isspace()

#define PORT 8888
#define MAX_CLIENTS 10
#define MAX_MESSAGE_SIZE 128 // Increased size to allow for protocol overhead

// Global array for connected client sockets.
int clientSocketList[MAX_CLIENTS];
// Mutex to protect access to clientSocketList.
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes.
int initializeListener();
void acceptConnection(int listeningSocket);
void broadcastChatMessage(char *messageToBroadcast, int senderSocket);
void processClientMessage(int clientSocket);
void *clientHandler(void *clientSocketPointer);

// This function parses the incoming protocol message and
// then broadcasts a formatted message in the form:
//   IPADDRESS [USERNAME] "MESSAGE"
void parseAndBroadcastProtocolMessage(const char *protocolMessage, int senderSocket)
{
    char clientIP[64] = "";
    char username[64] = "";
    int messageCount = -1;
    char messageText[256] = "";

    char temp[256];
    strncpy(temp, protocolMessage, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    // Tokenize using "|" as the delimiter.
    char *token = strtok(temp, "|");
    if (token != NULL)
    {
        strncpy(clientIP, token, sizeof(clientIP) - 1);
    }
    token = strtok(NULL, "|");
    if (token != NULL)
    {
        strncpy(username, token, sizeof(username) - 1);
    }
    token = strtok(NULL, "|");
    if (token != NULL)
    {
        messageCount = atoi(token);
    }
    token = strtok(NULL, "|");
    if (token != NULL)
    {
        // Remove surrounding quotes if present.
        size_t len = strlen(token);
        if (len >= 2 && token[0] == '\"' && token[len - 1] == '\"')
        {
            token[len - 1] = '\0';
            token++;
        }
        strncpy(messageText, token, sizeof(messageText) - 1);
    }

    // Format the final broadcast message.
    char broadcastMessage[512];
    snprintf(broadcastMessage, sizeof(broadcastMessage), "%s [%s] \"%s\"", clientIP, username, messageText);

    // Broadcast the message to all connected clients (including the sender).
    broadcastChatMessage(broadcastMessage, senderSocket);
    printf("DEBUG: Broadcasting: %s\n", broadcastMessage);
}

int main()
{
    int listeningSocket = initializeListener();

    // Initialize the global clientSocketList array.
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clientSocketList[i] = -1;
    }

    printf("Server listening on port %d\n", PORT);

    // Main loop: accept new connections.
    while (1)
    {
        acceptConnection(listeningSocket);
    }

    close(listeningSocket);
    return 0;
}

int initializeListener()
{
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int socketOption = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("socket binding failed!");
        exit(EXIT_FAILURE);
    }

    if (listen(listenSocket, MAX_CLIENTS) < 0)
    {
        perror("socket listen failed!");
        exit(EXIT_FAILURE);
    }

    return listenSocket;
}

// Accepts a new connection and spawns a thread to handle it.
void acceptConnection(int listenSocket)
{
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (clientSocket < 0)
    {
        perror("accept connection failed");
        return;
    }

    // Add the new client socket to the global list.
    pthread_mutex_lock(&clientMutex);
    int wasClientAdded = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clientSocketList[i] == -1)
        {
            clientSocketList[i] = clientSocket;
            wasClientAdded = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clientMutex);

    if (!wasClientAdded)
    {
        printf("DEBUG acceptConnection: Maximum clients reached. Rejecting connection.\n");
        close(clientSocket);
        return;
    }

    // Create a new thread for the client.
    pthread_t threadId;
    int *clientSocketPointer = malloc(sizeof(int));
    if (clientSocketPointer == NULL)
    {
        perror("malloc failed");
        close(clientSocket);
        return;
    }
    *clientSocketPointer = clientSocket;

    if (pthread_create(&threadId, NULL, clientHandler, clientSocketPointer) != 0)
    {
        perror("pthread_create failed");
        free(clientSocketPointer);
        close(clientSocket);
        pthread_mutex_lock(&clientMutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clientSocketList[i] == clientSocket)
            {
                clientSocketList[i] = -1;
                break;
            }
        }
        pthread_mutex_unlock(&clientMutex);
        return;
    }
    pthread_detach(threadId);

    printf("DEBUG acceptConnection: New connection, socket #%d\n", clientSocket);
}

// Broadcasts the given message to all connected clients (including the sender).
void broadcastChatMessage(char *messageToBroadcast, int senderSocket)
{
    pthread_mutex_lock(&clientMutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clientSocketList[i] != -1)
        {
            int sendResult = send(clientSocketList[i], messageToBroadcast, strlen(messageToBroadcast), 0);
            if (sendResult < 0)
            {
                perror("DEBUG broadcastChatMessage: send failed");
            }
        }
    }
    pthread_mutex_unlock(&clientMutex);
}

void processClientMessage(int clientSocket)
{
    char incomingMessage[MAX_MESSAGE_SIZE];

    while (1)
    {
        int numberOfBytesRead = read(clientSocket, incomingMessage, MAX_MESSAGE_SIZE - 1);
        if (numberOfBytesRead > 0)
        {
            incomingMessage[numberOfBytesRead] = '\0';

            // Remove trailing whitespace
            int end = numberOfBytesRead;
            while (end > 0 && isspace((unsigned char)incomingMessage[end - 1]))
            {
                incomingMessage[end - 1] = '\0';
                end--;
            }

            // Debug print the raw protocol message.
            printf("DEBUG: Received message from socket %d (len=%d): \"%s\"\n", clientSocket, numberOfBytesRead, incomingMessage);

            // Extract the protocol fields to get the actual message text.
            char temp[256];
            strncpy(temp, incomingMessage, sizeof(temp) - 1);
            temp[sizeof(temp) - 1] = '\0';

            // Protocol format: CLIENTIP|USERNAME|MESSAGECOUNT|"Message text"
            char *ipField = strtok(temp, "|");
            char *usernameField = strtok(NULL, "|");
            char *msgCountField = strtok(NULL, "|");
            char *messageField = strtok(NULL, "|");

            // Remove surrounding quotes from the messageField if present.
            if (messageField != NULL)
            {
                size_t len = strlen(messageField);
                if (len >= 2 && messageField[0] == '\"' && messageField[len - 1] == '\"')
                {
                    messageField[len - 1] = '\0';
                    messageField++;
                }
            }

            // If the extracted message text is ">>bye<<", disconnect.
            if (messageField && strcmp(messageField, ">>bye<<") == 0)
            {
                printf("DEBUG processClientMessage: Client on socket #%d requested disconnect.\n", clientSocket);
                break;
            }
            else
            {
                // Parse the full protocol message and broadcast the formatted message.
                parseAndBroadcastProtocolMessage(incomingMessage, clientSocket);
            }
        }
        else if (numberOfBytesRead == 0)
        {
            printf("Client on socket #%d disconnected.\n", clientSocket);
            break;
        }
        else
        {
            perror("read error");
            break;
        }
    }
}

// Thread function for handling a client's messages.
void *clientHandler(void *clientSocketPointer)
{
    int clientSocket = *(int *)clientSocketPointer;
    free(clientSocketPointer);

    processClientMessage(clientSocket);

    // Remove the client from the global list.
    pthread_mutex_lock(&clientMutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clientSocketList[i] == clientSocket)
        {
            clientSocketList[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&clientMutex);

    close(clientSocket);
    return NULL;
}
