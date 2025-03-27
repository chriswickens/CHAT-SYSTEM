/*

Things to do: Work on the protocol for both the client and the server
Protocol files in COMMON for server and client to have access to it
Functions in the protocol for client and server parsing

The client will need to determine if the message is from itself and change the << symbols

Client will parcel up messages AFTER they press enter - regardless of the message being over 40 characters
This will help prevent the server from doing this:

client 1: MESSAGE THAT IS OVER 40 CHAR PART 1
client 2: HELLO!
client 1: THE REST OF THE MESSAGE!


Check the test cases to ensure edge cases are accounted for
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

#define PORT 8888
#define MAX_CLIENTS 10

// Can be in common.h
#define MAX_MESSAGE_SIZE 65

// Global array to keep track of connected client sockets.
int clientSocketList[MAX_CLIENTS];
// Mutex to protect access to clientSocketList.
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
int initializeListener();
void acceptConnection(int listeningSocket);
void broadcastChatMessage(char *messageToBroadcast);
void processClientMessage(int clientSocket);
void *clientHandler(void *clientSocketPointer);

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

    // Allow immediate reuse of the address after server termination.
    int socketOption = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Prepare the server address.
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

    // Grab the mutex for clients that are connecting to add them to the client list
    pthread_mutex_lock(&clientMutex);

    // Used to check if the client was successfully added to the list of clients
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

    // Create a new thread for the client trying to connect
    pthread_t threadId;

    // Allocate space for the size of an int for the client socket
    int *clientSocketPointer = malloc(sizeof(int));

    if (clientSocketPointer == NULL)
    {
        perror("malloc failed");
        close(clientSocket);
        return;
    }

    // Assign the pointer the socket for the client
    *clientSocketPointer = clientSocket;

    if (pthread_create(&threadId, NULL, clientHandler, clientSocketPointer) != 0)
    {
        perror("pthread_create failed");
        free(clientSocketPointer);
        close(clientSocket);
        pthread_mutex_lock(&clientMutex);
        // Remove the client from the global list.
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
    // Detach the thread so resources are freed on exit.
    pthread_detach(threadId);

    printf("DEBUG acceptConnection: New connection, socket #%d\n", clientSocket);
}

// Broadcasts a chat message to all connected clients.
void broadcastChatMessage(char *messageToBroadcast)
{
    // Simulate a user name.
    char userName[] = "MSG From Server";
    char combinedMessage[MAX_MESSAGE_SIZE + sizeof(userName) + 3]; // username, ": ", message, null terminator
    snprintf(combinedMessage, sizeof(combinedMessage), "%s: %s", userName, messageToBroadcast);

    // Grab the mutex to check the list of clients and send the message to any clients currently connected
    pthread_mutex_lock(&clientMutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clientSocketList[i] != -1)
        {
            int sendMessageResult = send(clientSocketList[i], combinedMessage, strlen(combinedMessage), 0);
            if (sendMessageResult < 0)
            {
                perror("DEBUG broadcastChatMessage: send failed");
            }
        }
    }
    pthread_mutex_unlock(&clientMutex);
}

// Processes messages from a single client. This function runs in a dedicated thread.
void processClientMessage(int clientSocket)
{
    char incomingMessage[MAX_MESSAGE_SIZE];

    while (1)
    {
        // This is where you receive the message INCLUDING THE PROTOCOL DETAILS
        // This will be the ACTUAL message + the protocol details
        int numberOfBytesRead = read(clientSocket, incomingMessage, MAX_MESSAGE_SIZE - 1);

        if (numberOfBytesRead > 0)
        {

            // THIS IS WHERE YOU NEED TO DO PROTOCOL PARSING!

            // Check if it is a FULL_MESSAGE, PART_ONE or PART_TWO
            // Create an array that can hold a full 80 character message
            // If it is part two, put it in the second half
            // if it is part ONE put it in the first half
            // In the ELSE statement below you can then iterate over the char array with the full message to send
            // back to the client and split it up into two separate messages including the protocol needed for the client

            // This will probably need to be +1 to properly null terminate the ACTUAL message!
            incomingMessage[numberOfBytesRead] = '\0'; // Null-terminate the string.

            char incomingProtocol[MAX_MESSAGE_SIZE] = "100.100.100|KyleM|0|Hello World!";
            printf("Server stuff!\n\n");
            // Split protocol message into vars with strtok
            char *token;
            char *ip;
            token = strtok(incomingProtocol, "|");
            if (ip == NULL)
            {
                printf("Error parsing ip with strtok!\n");
                exit(-1);
            }
            ip = token;

            char *username;
            username = strtok(NULL, "|");
            if (username == NULL)
            {
                printf("Error parsing username with strtok!\n");
                exit(-1);
            }

            char *bit;
            bit = strtok(NULL, "|");
            if (bit == NULL)
            {
                printf("Error parsing bit with strtok!\n");
                exit(-1);
            }

            char *message;
            message = strtok(NULL, "|");
            if (message == NULL)
            {
                printf("Error parsing message with strtok!\n");
                exit(-1);
            }

            printf("Ip: %s\n", ip);
            printf("Hello!\n");
            printf("Username: %s\n", username);
            printf("Bit: %s\n", bit);
            printf("Message: %s\n", message);

            // If the client sends "quit", disconnect.
            if (strcmp(incomingMessage, ">>bye<<") == 0)
            {
                printf("DEBUG processClientMessage: Client on socket #%d requested disconnect.\n", clientSocket);
                break;
            }
            else
            {
                // Iterate over the FULL message and split it then send each part
                // This will require a SHORT delay using usleep() if the message is split
                // to ensure that we are not trying to send two messages at the exact same time
                // Maybe use a BOOL that is changed like oneMessage = true when its one message
                // and if its two messages, change that to false, then using that, send both
                // halfs to the server with the delay between the first and second one
                broadcastChatMessage(incomingMessage);
            }
        }

        else if (numberOfBytesRead == 0)
        {
            // Client disconnected.
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
            //
            clientSocketList[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&clientMutex);

    close(clientSocket);
    return NULL;
}
