#include "../inc/chat-server.h"

// Global array for connected client sockets.
int clientSocketList[MAX_CLIENTS];

// Mutex to protect access to clientSocketList.
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * FUNCTION : parseAndBroadcastProtocolMessage
 *
 * DESCRIPTION : This function parses the message from a client, gets the client IP, username, message count, and message text,
 * formats a a return message, and then calls broadcastChatMessage to send the message to all clients
 *
 * PARAMETERS : const char *protocolMessage : The raw protocol message string.
 *              int senderSocket : The socket of the sender client.
 *
 * RETURNS : void
 */
void parseAndBroadcastProtocolMessage(const char *protocolMessage, int senderSocket)
{
    char clientIP[64] = "";     // Storage for IP
    char username[64] = "";     // Storage for username
    int messageCount = -1;      // To check the message count from client
    char messageText[256] = ""; // To store the message text from the client

    char temporaryMessageSpace[256]; // Temp storage when splitting the message
    strncpy(temporaryMessageSpace, protocolMessage, sizeof(temporaryMessageSpace) - 1);
    temporaryMessageSpace[sizeof(temporaryMessageSpace) - 1] = '\0';

    // Pull out the IP address
    char *token = strtok(temporaryMessageSpace, "|");
    if (token != NULL)
    {
        strncpy(clientIP, token, sizeof(clientIP) - 1);
        // printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the IP: %s\n", clientIP);
    }

    // Pull the username
    token = strtok(NULL, "|");
    if (token != NULL)
    {
        strncpy(username, token, sizeof(username) - 1);
        // printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the USERNAME: %s\n", username);
    }

    // Pull the message COUNT (to check if it is one message up to 40 chars, or parts of an 80 char message)
    token = strtok(NULL, "|");
    if (token != NULL)
    {

        messageCount = atoi(token);
        // printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the MSG COUNT: %i\n", messageCount);
    }

    // Pull the message
    token = strtok(NULL, "|");

    if (token != NULL)
    {
        strncpy(messageText, token, sizeof(messageText) - 1);
        // printf("DEBUG : parseAndBroadcastProtocolMessage() - Got the MESSAGE: %s\n", messageText);
        // printf("Literal Message Size: %li\n", strlen(messageText));
    }

    // Format the final broadcast message.
    char broadcastMessage[512];
    snprintf(broadcastMessage, sizeof(broadcastMessage), "%-*s [%-*s] >> %-*s", 1, clientIP, 5, username, 41, messageText);

    // Broadcast the message to all connected clients
    broadcastChatMessage(broadcastMessage, senderSocket);

    // printf("\nDEBUG PARSE COMPLETE: Broadcasting: %s\n", broadcastMessage);
    // printf("-------Parsing INCOMING message-------\n\n");
}

/*
 * FUNCTION : initializeListener
 *
 * DESCRIPTION : This function creates a listening socket, retrieves the local host details,
 * sets socket options, binds the socket to the server port, and begins listening for incoming connections.
 *
 * PARAMETERS : None
 *
 * RETURNS : int : The listening socket descriptor on success, or exits on failure.
 */
int initializeListener()
{
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    char host[256];
    // char *clientIP;
    struct hostent *hostDetails;
    int hostname;

    // Get the literal (local) host name
    hostname = gethostname(host, sizeof(host));
    if (hostname < 0)
    {
        perror("gethostname failed");
        exit(EXIT_FAILURE);
    }
    // printf("Local host name (from gethostname): %s\n", host);

    // Retrieve host details using the literal host name
    hostDetails = gethostbyname(host);
    if (hostDetails == NULL)
    {
        herror("gethostbyname failed");
        exit(EXIT_FAILURE);
    }

    // Print the host name debug
    // printf("LITERAL host name: %s\n", hostDetails->h_name);
    // clientIP = inet_ntoa(*((struct in_addr *)hostDetails->h_addr_list[0]));
    // printf("Primary IP: %s\n", clientIP);

    int socketOption = 1;
    // Set socket options (REUSEADDR so it wont get stuck)
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Create the socket address details to bind to
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    // Bind to the socket using socketAddress details
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("socket binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listenSocket, MAX_CLIENTS) < 0)
    {
        perror("socket listen failed");
        exit(EXIT_FAILURE);
    }

    return listenSocket;
}

/*
 * FUNCTION : acceptConnection
 *
 * DESCRIPTION : This function accepts an incoming connection,
 * it adds the new client to the global clientSocketList array, and creates a new thread to handle client messages.
 *
 * PARAMETERS : int listenSocket : The listening socket descriptor.
 *
 * RETURNS : void
 */
void acceptConnection(int listenSocket)
{
    // Struct for client details
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);

    // Accept the client connection
    int clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (clientSocket < 0)
    {
        perror("accept connection failed");
        return;
    }

    // Add the new client socket to the list
    // Get the mutex
    pthread_mutex_lock(&clientMutex);

    // Check for if the client was added successfully
    int wasClientAdded = 0;

    // Check the list of clients
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

    // Too many clients exist (max of 10)
    if (!wasClientAdded)
    {
        // printf("DEBUG acceptConnection: Maximum clients reached. Rejecting connection.\n");
        close(clientSocket);
        return;
    }

    // Create a new thread for the client.
    pthread_t threadId;

    // Allocate memory for the socket pointer
    int *clientSocketPointer = malloc(sizeof(int));
    if (clientSocketPointer == NULL)
    {
        perror("malloc failed");
        close(clientSocket);
        return;
    }

    // Create a pointer for the socket
    *clientSocketPointer = clientSocket;

    // Create the thread, call clientHandler, pass in the socket
    if (pthread_create(&threadId, NULL, clientHandler, clientSocketPointer) != 0)
    {
        perror("pthread_create failed");
        free(clientSocketPointer);
        close(clientSocket);
        // get the mutex
        pthread_mutex_lock(&clientMutex);

        // Check the list of clients
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

    // printf("DEBUG acceptConnection: New connection, socket #%d\n", clientSocket);
}

/*
 * FUNCTION : broadcastChatMessage
 *
 * DESCRIPTION : This function broadcasts a message to all connected clients by iterating through the global
 * clientSocketList array and sending the message
 *
 * PARAMETERS : char *messageToBroadcast : The message to broadcast.
 *              int senderSocket : The socket descriptor of the sender.
 *
 * RETURNS : void
 */
void broadcastChatMessage(char *messageToBroadcast, int senderSocket)
{
    printf("Send messagE: %s", messageToBroadcast);
    // Get the mutex
    pthread_mutex_lock(&clientMutex);

    // Check the client list
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // If there is a client there
        if (clientSocketList[i] != -1)
        {
            // Send the message to the client
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
 * DESCRIPTION : This function keeps reading messages from a client, processes them,
 * and triggers a broadcast or disconnect if necessary
 *
 * PARAMETERS : int clientSocket : The client socket descriptor.
 *
 * RETURNS : void
 */
void processClientMessage(int clientSocket)
{
    char incomingMessage[MAX_PROTOL_MESSAGE_SIZE];

    // Keep checking for messages from clients
    while (1)
    {
        int numberOfBytesRead = read(clientSocket, incomingMessage, MAX_PROTOL_MESSAGE_SIZE - 1);
        if (numberOfBytesRead > 0)
        {
            incomingMessage[numberOfBytesRead] = '\0';

            // printf("\n------- GOT MESSAGE FROM CLIENT ------\nprocessClientMessage() Start\n");
            // Debug print the raw protocol message.
            // printf("DEBUG: Received message from socket %d (len=%d): \"%s\"\n", clientSocket, numberOfBytesRead, incomingMessage);

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
                // printf("DEBUG processClientMessage: Client on socket #%d requested disconnect.\n", clientSocket);
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
            // printf("Client on socket #%d disconnected.\n", clientSocket);
            break;
        }
        else
        {
            perror("read error");
            break;
        }
    }
    // printf("\n------- END GOT MESSAGE FROM CLIENT ------\nprocessClientMessage() FINISH\n");
}

/*
 * FUNCTION : clientHandler
 *
 * DESCRIPTION : This function takes a client socket and starts the message processing for it
 *
 * PARAMETERS : void *clientSocketPointer : Pointer to the client socket descriptor (cast from int *).
 *
 * RETURNS : void * : Always returns NULL.
 */
void *clientHandler(void *clientSocketPointer)
{
    // Cast the pointer to an int
    int clientSocket = *(int *)clientSocketPointer;
    free(clientSocketPointer);

    processClientMessage(clientSocket);

    // Remove the client from the list
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

int main()
{
    int listeningSocket = initializeListener();

    // Initialize the global clientSocketList array to store client information
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // Set all entries to -1 (for checking later, if -1, that means no client exists at this index!)
        clientSocketList[i] = -1;
    }

    // printf("Server listening on port %d\n", SERVER_PORT);

    // Start accepting connections
    while (1)
    {
        acceptConnection(listeningSocket);
    }

    close(listeningSocket);
    return 0;
}
