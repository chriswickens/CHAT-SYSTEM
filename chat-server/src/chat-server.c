#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define MAX_MESSAGE_SIZE 80

// Function prototypes
int initializeListener();
void acceptConnection(int listeningSocket, struct pollfd socketList[], int *totalSocketCount);
void broadcastChatMessage(struct pollfd socketList[], int totalSocketCount, char *messageToBroadcast);
void processClientMessage(struct pollfd socketList[], int totalSocketCount);

int main()
{
    int listeningSocket, totalSocketCount = 1;
    struct pollfd socketList[MAX_CLIENTS + 1]; // socketList[0] is for the listening socket (+1 for max clients AND the listener socket)
    int timeout = 5000; // poll timeout in milliseconds

    // Initialize the listening socket.
    listeningSocket = initializeListener();
    // Assign the listening socket to index 0 of the socket array
    socketList[0].fd = listeningSocket;

    socketList[0].events = POLLIN;

    for (int i = 1; i <= MAX_CLIENTS; i++)
    {
        socketList[i].fd = -1; // Mark remaining slots as unused.
    }

    printf("Server listening on port %d\numberOfBytesRead", PORT);

    while (1)
    {
        int returnCode = poll(socketList, totalSocketCount, timeout);
        if (returnCode < 0)
        {
            perror("Error during polling!");
            break;
        }
        if (returnCode == 0)
        {
            // Timeout: no events.
            continue;
        }

        // Accept new connections if available.
        if (socketList[0].revents & POLLIN)
        {
            acceptConnection(listeningSocket, socketList, &totalSocketCount);
        }

        // Process data from connected clients.
        processClientMessage(socketList, totalSocketCount);
    }

    close(listeningSocket);
    return 0;
}


// This code does not let the server use an address that has already been bound
// If this is used, when you CTRL+C the server terminal, you need to wait 5000ms for the server
// to stop listening on the socket
// int initializeListener()
// {
//     int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
//     if (listenSocket < 0)
//     {
//         perror("socket failed");
//         exit(EXIT_FAILURE);
//     }

//     // Set the socket to non-blocking mode.
//     int flags = fcntl(listenSocket, F_GETFL, 0);
//     if (flags < 0)
//     {
//         perror("fcntl F_GETFL failed");
//         exit(EXIT_FAILURE);
//     }
//     fcntl(listenSocket, F_SETFL, flags | O_NONBLOCK);

//     // Prepare the server address.
//     struct sockaddr_in serverAddress;
//     memset(&serverAddress, 0, sizeof(serverAddress));
//     serverAddress.sin_family = AF_INET;
//     serverAddress.sin_addr.s_addr = INADDR_ANY;
//     serverAddress.sin_port = htons(PORT);

//     if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
//     {
//         perror("bind failed");
//         exit(EXIT_FAILURE);
//     }

//     if (listen(listenSocket, MAX_CLIENTS) < 0)
//     {
//         perror("listen failed");
//         exit(EXIT_FAILURE);
//     }

//     return listenSocket;
// }

// Refined version of the above that lets the server bind to the an address already in use
int initializeListener()
{
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow reusing the address immediately after the server terminates.
    int socketOption = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set the socket to non-blocking mode.
    int flags = fcntl(listenSocket, F_GETFL, 0);
    if (flags < 0)
    {
        perror("Error getting flags!");
        exit(EXIT_FAILURE);
    }
    fcntl(listenSocket, F_SETFL, flags | O_NONBLOCK);

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

// Accepts the connection from a client
void acceptConnection(int listenSocket, struct pollfd socketList[], int *numberOfSockets)
{
    // Struct to hold the client address details
    struct sockaddr_in clientAddress;

    // The length in bytes of the address
    socklen_t clientStructLength = sizeof(clientAddress);

    // Get the result of the accept() function
    int connectionResult = accept(listenSocket, (struct sockaddr *)&clientAddress, &clientStructLength);

    if (connectionResult < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
            perror("accept connection failed!");
        return;
    }

    // Set the new client socket to non-blocking.
    int flags = fcntl(connectionResult, F_GETFL, 0);
    fcntl(connectionResult, F_SETFL, flags | O_NONBLOCK);

    // Add the new connection to the pollfd array.
    // Declare the for loop iterator outside of the loop to allow access further down
    int i;
    
    // Start at 1 to skip the listener socket in the server
    for (i = 1; i <= MAX_CLIENTS; i++)
    {
        if (socketList[i].fd < 0)
        {
            socketList[i].fd = connectionResult;
            socketList[i].events = POLLIN;
            
            // If the current number of sockets is greater than or equal to the actual number of sockets
            // Increment number of sockets to represent a new connection
            if (i >= *numberOfSockets)
            {
                *numberOfSockets = i + 1;
            }

            // DEBUG
            printf("DEBUG acceptConnection() : New connection, fd: %d\numberOfBytesRead", connectionResult);
            break;
        }
    }

    // If the max clients will be exceeded, close the connection to the client trying to connect
    if (i > MAX_CLIENTS)
    {
        // Too many connections.
        printf("DEBUG acceptConnection() : Maximum clients reached. Rejecting connection.\numberOfBytesRead");
        close(connectionResult);
    }
}

// Sends the messages to the sockets of each connected client
void broadcastChatMessage(struct pollfd socketList[], int numberOfSockets, char *messageToBroadcast)
{
    // This is where we will need to implement the protocol parsing to send a message containing
    // necessary information back to all the clients
    // Simulate retrieving a user name.
    char userName[10] = "Wickens";
    int nameLength = strlen(userName);

    // Allocate space for the combined messageToBroadcast: username, ": ", messageToBroadcast, and null terminator.
    char combinedMessage[MAX_MESSAGE_SIZE + nameLength + 3];

    strcpy(combinedMessage, userName);
    strcat(combinedMessage, ": ");
    strcat(combinedMessage, messageToBroadcast);

    int combinedLength = strlen(combinedMessage);

    // Iterate over the client sockets (start at 1 to skip server listener socket)
    for (int i = 1; i < numberOfSockets; i++)
    {
        if (socketList[i].fd >= 0)
        {
            int sent = send(socketList[i].fd, combinedMessage, combinedLength, 0);

            // If NO messages were sent, there was a problemo!
            if (sent < 0)
            {
                perror("DEBUG broadcastChatMessage() : send failed");
            }
        }
    }
}

void processClientMessage(struct pollfd socketList[], int numberOfSockets)
{
    char buffer[MAX_MESSAGE_SIZE];

    // Iterate over the client sockets (start at 1 to skip server listener socket)
    for (int i = 1; i < numberOfSockets; i++)
    {
        if (socketList[i].fd < 0)
        {
            continue;
        }

        if (socketList[i].revents & POLLIN)
        {
            int numberOfBytesRead = read(socketList[i].fd, buffer, MAX_MESSAGE_SIZE - 1);

            if (numberOfBytesRead > 0)
            {
                buffer[numberOfBytesRead] = '\0'; // Null-terminate the string.

                // Check for a specific messageToBroadcast (e.g., "quit") to remove the client.
                // THIS NEEDS TO BE PUT INTO A FUNCTION TO CLEAN UP THE SOCKET LIST
                // Function  header: int cleanSockets(int socketIndex, struct pollfd socketList[]);
                // Returns an INT that dictates if it succeeded or failed
                if (strcmp(buffer, "quit") == 0)
                {
                    printf("DEBUG processClientMessage() : Client on fd %d requested disconnect.\numberOfBytesRead", socketList[i].fd);
                    close(socketList[i].fd);
                    socketList[i].fd = -1;
                }

                else
                {
                    broadcastChatMessage(socketList, numberOfSockets, buffer);
                }
            }
            else if (numberOfBytesRead == 0)
            {
                // Client disconnected.
                printf("Client on fd %d disconnected.\numberOfBytesRead", socketList[i].fd);
                close(socketList[i].fd);
                socketList[i].fd = -1;
            }
            else
            {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    perror("read error");
                    close(socketList[i].fd);
                    socketList[i].fd = -1;
                }
            }
        }
    }
}
