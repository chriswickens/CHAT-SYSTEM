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
int init_listening_socket();
void accept_new_connection(int listeningSocket, struct pollfd socketList[], int *totalSocketCount);
void broadcast_message(struct pollfd socketList[], int totalSocketCount, char *messageToBroadcast);
void handle_client_data(struct pollfd socketList[], int totalSocketCount);

int main()
{
    int listeningSocket, totalSocketCount = 1;
    struct pollfd socketList[MAX_CLIENTS + 1]; // socketList[0] is for the listening socket
    int timeout = 5000;                        // poll timeout in milliseconds

    // Initialize the listening socket.
    listeningSocket = init_listening_socket();
    // Assign the listening socket to index 0 of the socket array
    socketList[0].fd = listeningSocket;

    socketList[0].events = POLLIN;

    for (int i = 1; i <= MAX_CLIENTS; i++)
    {
        socketList[i].fd = -1; // Mark remaining slots as unused.
    }

    printf("Server listening on port %d\n", PORT);

    while (1)
    {
        int ret = poll(socketList, totalSocketCount, timeout);
        if (ret < 0)
        {
            perror("poll failed");
            break;
        }
        if (ret == 0)
        {
            // Timeout: no events.
            continue;
        }

        // Accept new connections if available.
        if (socketList[0].revents & POLLIN)
        {
            accept_new_connection(listeningSocket, socketList, &totalSocketCount);
        }

        // Process data from connected clients.
        handle_client_data(socketList, totalSocketCount);
    }

    close(listeningSocket);
    return 0;
}

int init_listening_socket()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set the socket to non-blocking mode.
    int flags = fcntl(listen_fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("fcntl F_GETFL failed");
        exit(EXIT_FAILURE);
    }
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    // Prepare the server address.
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, MAX_CLIENTS) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    return listen_fd;
}

void accept_new_connection(int listen_fd, struct pollfd socketList[], int *nfds)
{
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
    if (conn_fd < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
            perror("accept failed");
        return;
    }

    // Set the new client socket to non-blocking.
    int flags = fcntl(conn_fd, F_GETFL, 0);
    fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);

    // Add the new connection to the pollfd array.
    int i;
    for (i = 1; i <= MAX_CLIENTS; i++)
    {
        if (socketList[i].fd < 0)
        {
            socketList[i].fd = conn_fd;
            socketList[i].events = POLLIN;
            if (i >= *nfds)
            {
                *nfds = i + 1;
            }
            printf("New connection, fd: %d\n", conn_fd);
            break;
        }
    }
    if (i > MAX_CLIENTS)
    {
        // Too many connections.
        printf("Maximum clients reached. Rejecting connection.\n");
        close(conn_fd);
    }
}

void broadcast_message(struct pollfd socketList[], int nfds, char *messageToBroadcast)
{
    // Simulate retrieving a user name.
    char userName[10] = "Wickens";
    int nameLength = strlen(userName);
    // Allocate space for the combined messageToBroadcast: username, ": ", messageToBroadcast, and null terminator.
    char combinedMessage[MAX_MESSAGE_SIZE + nameLength + 3];

    strcpy(combinedMessage, userName);
    strcat(combinedMessage, ": ");
    strcat(combinedMessage, messageToBroadcast);

    int combinedLength = strlen(combinedMessage);

    for (int j = 1; j < nfds; j++)
    {
        if (socketList[j].fd >= 0)
        {
            int sent = send(socketList[j].fd, combinedMessage, combinedLength, 0);
            if (sent < 0)
            {
                perror("send failed");
            }
        }
    }
}

void handle_client_data(struct pollfd socketList[], int nfds)
{
    char buffer[MAX_MESSAGE_SIZE];
    for (int i = 1; i < nfds; i++)
    {
        if (socketList[i].fd < 0)
            continue;

        if (socketList[i].revents & POLLIN)
        {
            int n = read(socketList[i].fd, buffer, MAX_MESSAGE_SIZE - 1);


            if (n > 0)
            {
                buffer[n] = '\0'; // Null-terminate the string.

                // Check for a specific messageToBroadcast (e.g., "quit") to remove the client.
                // THIS NEEDS TO BE PUT INTO A FUNCTION TO CLEAN UP THE SOCKET LIST
                // Function  header: int cleanSockets(int socketIndex, struct pollfd socketList[]);
                // Returns an INT that dictates if it succeeded or failed
                if (strcmp(buffer, "quit") == 0)
                {
                    printf("Client on fd %d requested disconnect.\n", socketList[i].fd);
                    close(socketList[i].fd);
                    socketList[i].fd = -1;
                }

                else
                {
                    broadcast_message(socketList, nfds, buffer);
                }
            }
            else if (n == 0)
            {
                // Client disconnected.
                printf("Client on fd %d disconnected.\n", socketList[i].fd);
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
