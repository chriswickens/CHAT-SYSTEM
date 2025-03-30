/*
FILE : chat-server.c
PROJECT : SENG2030 - Assignment #4
PROGRAMMER : Volfer Carvalho Freire, Jack Prudnikowicz, Kyle Murawsky, Chris Wickens, Melissa Reyes
FIRST VERSION : 2025-03-25
DESCRIPTION :
Contains common functions and variables used by various programs in this system
*
*/

#include "../inc/chat-server.h"

// Global array for connected client sockets.
int clientSocketList[MAX_CLIENTS];

// Mutex to protect access to clientSocketList.
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

/*
* FUNCTION : parseAndBroadcastProtocolMessage
*
* DESCRIPTION : Parse a protocol formatted message and broadcast the formatted message to all connected clients
*
* PARAMETERS : const char *protocolMessage : the protocol message string received from a client
* int senderSocket : the socket descriptor of the sender
*
* RETURNS : void
*/
void parseAndBroadcastProtocolMessage(const char *protocolMessage, int senderSocket)
{
    printf("\n-------Parsing INCOMING message-------\n");
    char clientIP[64] = ""; // Storage for IP
    char username[64] = ""; // Storage for username
    int messageCount = -1; // To check the message count from client
    char messageText[256] = ""; // To store the message text from the client

    char temporaryMessageSpace[256]; // Temp storage when splitting the message
    strncpy(temporaryMessageSpace, protocolMessage, sizeof(temporaryMessageSpace) - 1);
    temporaryMessageSpace[sizeof(temporaryMessageSpace) - 1] = '\0';

    // Tokenize using "|" as the delimiter.
    // Pull out the IP address
    char *token = strtok(temporaryMessageSpace, "|");
    if (token != NULL)
    {
        strncpy(clientIP, token, sizeof(clientIP) - 1);
        printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the IP: %s\n", clientIP);
    }

    // Pull the username
    token = strtok(NULL, "|");
    if (token != NULL)
    {
        strncpy(username, token, sizeof(username) - 1);
        printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the USERNAME: %s\n", username);
    }

    // Pull the message COUNT (to check if it is one message up to 40 chars, or parts of an 80 char message)
    token = strtok(NULL, "|");
    if (token != NULL)
    {
        messageCount = atoi(token);
        printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the MSG COUNT: %i\n", messageCount);
    }

    // Pull the message
    token = strtok(NULL, "|");

    if (token != NULL)
    {
        strncpy(messageText, token, sizeof(messageText) - 1);
        printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the MESSAGE: %s\n", messageText);
        printf("Literal Message Size: %li\n", strlen(messageText));
    }

    // Format the final broadcast message.
    char broadcastMessage[512];
    snprintf(broadcastMessage, sizeof(broadcastMessage), "%-*s [%-*s] >> %-*s", 14, clientIP, 5, username, 41, messageText);

    // Broadcast the message to all connected clients
    broadcastChatMessage(broadcastMessage, senderSocket);
    printf("\nDEBUG PARSE COMPLETE: Broadcasting: %s\n", broadcastMessage);
    printf("-------Parsing INCOMING message-------\n\n");
}

/*
* FUNCTION : initializeListener
*
* DESCRIPTION : Create a listening socket for the server, bind it to the specified port, and start listening for connections
*
* PARAMETERS : void
*
* RETURNS : int : the listening socket descriptor or exits on failure
*/
int initializeListener() {
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    char host[256];
    char *clientIP;
    struct hostent *hostDetails;
    int hostname;

    // Get the literal (local) host name
    hostname = gethostname(host, sizeof(host));
    if (hostname < 0) {
        perror("gethostname failed");
        exit(EXIT_FAILURE);
    }
    printf("Local host name (from gethostname): %s\n", host);

    // Retrieve host details using the literal host name
    hostDetails = gethostbyname(host);
    if (hostDetails == NULL) {
        herror("gethostbyname failed");
        exit(EXIT_FAILURE);
    }

    // Print the canonical host name
    printf("LITERAL host name: %s\n", hostDetails->h_name);

    clientIP = inet_ntoa(*((struct in_addr *)hostDetails->h_addr_list[0]));
    printf("Primary IP: %s\n", clientIP);

    int socketOption = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("socket binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listenSocket, MAX_CLIENTS) < 0) {
        perror("socket listen failed");
        exit(EXIT_FAILURE);
    }

    return listenSocket;
}

/*
* FUNCTION : acceptConnection
*
* DESCRIPTION : Accept a new client connection, add it to the global client list, and spawn a new thread to handle it
*
* PARAMETERS : int listenSocket : the listening socket descriptor
*
* RETURNS : void
*/
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

/*
* FUNCTION : broadcastChatMessage
*
* DESCRIPTION : Broadcast a chat message to all connected clients
*
* PARAMETERS : char *messageToBroadcast : the message to send
* int senderSocket : the sender's socket descriptor (unused in the broadcast)
*
* RETURNS : void
*/
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

/*
* FUNCTION : processClientMessage
*
* DESCRIPTION : Process messages received from a client socket and disconnect if a disconnect message is received
*
* PARAMETERS : int clientSocket : the client socket descriptor from which to read messages
*
* RETURNS : void
*/
void processClientMessage(int clientSocket)
{
    char incomingMessage[MAX_PROTOL_MESSAGE_SIZE];

    while (1)
    {
        int numberOfBytesRead = read(clientSocket, incomingMessage, MAX_PROTOL_MESSAGE_SIZE - 1);
        if (numberOfBytesRead > 0)
        {
            incomingMessage[numberOfBytesRead] = '\0';

            printf("\n------- GOT MESSAGE FROM CLIENT ------\nprocessClientMessage() Start\n");
            // Debug print the raw protocol message.
            printf("DEBUG: Received message from socket %d (len=%d): \"%s\"\n", clientSocket, numberOfBytesRead, incomingMessage);

            // Extract the protocol fields to get the actual message text.
            char temporaryMessageSpace[256];
            strncpy(temporaryMessageSpace, incomingMessage, sizeof(temporaryMessageSpace) - 1);
            temporaryMessageSpace[sizeof(temporaryMessageSpace) - 1] = '\0';

            // Protocol format: CLIENTIP|USERNAME|MESSAGECOUNT|"Message text"
            // Get the IP
            char *ipField = strtok(temporaryMessageSpace, "|");
            // Get the user name
            char *usernameField = strtok(NULL, "|");
            // Get the message count
            char *msgCountField = strtok(NULL, "|");
            // Get the message
            char *messageField = strtok(NULL, "|");

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
    printf("\n------- END GOT MESSAGE FROM CLIENT ------\nprocessClientMessage() FINISH\n");
}

/*
* FUNCTION : clientHandler
*
* DESCRIPTION : Handle a client connection in a separate thread, process its messages, and remove it upon disconnect
*
* PARAMETERS : void *clientSocketPointer : pointer to an integer holding the client socket descriptor
*
* RETURNS : void * : always returns NULL
*/
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

/*
* FUNCTION : main
*
* DESCRIPTION : Main function to initialize the server listener, accept incoming connections, and manage client communication
*
* PARAMETERS : void
*
* RETURNS : int : exit status of the program
*/
int main()
{
    int listeningSocket = initializeListener();

    // Initialize the global clientSocketList array.
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clientSocketList[i] = -1;
    }

    printf("Server listening on port %d\n", SERVER_PORT);

    // Main loop: accept new connections.
    while (1)
    {
        acceptConnection(listeningSocket);
    }

    close(listeningSocket);
    return 0;
}
