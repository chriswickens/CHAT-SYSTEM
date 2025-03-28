/*
 * chat-client.c
 *
 * Updated to use ncurses for I/O and to send messages using a custom protocol:
 *   CLIENTIPADDRESS|CLIENTUSERNAME|MESSAGECOUNT|"Message text"
 *
 * If a message exceeds 40 characters, it is split into two parts:
 *   MESSAGECOUNT 1 for the first part, and MESSAGECOUNT 2 for the second.
 * The server then parses this protocol message and broadcasts
 * the formatted message (e.g., 127.0.0.1 [Chris] "FUCK THIS ALL")
 * to every connected client (including the sender).
 *
 * When receiving a message, the client checks if the IP in the message
 * matches its own. If so, it appends a plus sign (+) to the end of the message.
 */

#include "../inc/chat-client.h"

// some of these belong in the common.h file
// #define SERVER_PORT 8888      // Port number of chat server
// #define MAX_MESSAGE_SIZE 81   // Maximum length of a user-typed message
// #define MAX_PROTOCOL_SIZE 128 // Buffer size for protocol messages
// #define MAX_PART_LEN 40       // Maximum characters per message part

int socketFileDescriptor;                // Global socket descriptor
WINDOW *messageWindow, *userInputWindow; // ncurses windows for chat display and input
char receiveBuffer[MAX_PROTOL_MESSAGE_SIZE];   // Buffer for incoming messages

char clientIP[INET_ADDRSTRLEN]; // Stores the client's IP address

// Function prototypes.
// These belong in the chat-client.h file
void initializeNcursesWindows(void);
int connectToServer(const char *serverIpAddress);
void *handleReceivedMessage(void *arg);
int startReceivingThread(void);
void handleUserInput(char *userName, char *clientIP);
void cleanup(void);
void getLocalIP(int sockfd, char *ipBuffer, size_t bufferSize);
void splitMessage(const char *fullString, char *firstPart, char *secondPart);

// Get the local IP address from the connected socket.
void getLocalIP(int sockfd, char *ipBuffer, size_t bufferSize)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &addr.sin_addr, ipBuffer, bufferSize);
    }
    else
    {
        strncpy(ipBuffer, "0.0.0.0", bufferSize);
    }
}

// Splitting function based on the provided algorithm.
void splitMessage(const char *fullString, char *firstPart, char *secondPart)
{
    int fullStringLength = strlen(fullString);
    if (fullStringLength <= CLIENT_MSG_PART_LENGTH)
    {
        strncpy(firstPart, fullString, CLIENT_MSG_PART_LENGTH);
        firstPart[CLIENT_MSG_PART_LENGTH] = '\0';
        secondPart[0] = '\0';
        return;
    }
    int minSplit = fullStringLength - CLIENT_MSG_PART_LENGTH;
    int maxSplit = CLIENT_MSG_PART_LENGTH;
    int midPoint = fullStringLength / 2;
    int splitIndex = -1;
    for (int offset = 0; offset <= (maxSplit - minSplit); offset++)
    {
        int lower = midPoint - offset;
        if (lower >= minSplit && lower <= maxSplit && fullString[lower] == ' ')
        {
            splitIndex = lower;
            break;
        }
        int upper = midPoint + offset;
        if (upper >= minSplit && upper <= maxSplit && fullString[upper] == ' ')
        {
            splitIndex = upper;
            break;
        }
    }
    if (splitIndex == -1)
        splitIndex = midPoint;
    if (fullString[splitIndex] == ' ')
    {
        strncpy(firstPart, fullString, splitIndex);
        firstPart[splitIndex] = '\0';
        int secondLen = fullStringLength - (splitIndex + 1);
        if (secondLen > CLIENT_MSG_PART_LENGTH)
        {
            secondLen = CLIENT_MSG_PART_LENGTH;
        }
            
        strncpy(secondPart, fullString + splitIndex + 1, secondLen);
        secondPart[secondLen] = '\0';
    }
    else
    {
        strncpy(firstPart, fullString, splitIndex);
        firstPart[splitIndex] = '\0';
        int secondLen = fullStringLength - splitIndex;
        if (secondLen > CLIENT_MSG_PART_LENGTH)
        {
             secondLen = CLIENT_MSG_PART_LENGTH;
        }
           
        strncpy(secondPart, fullString + splitIndex, secondLen);
        secondPart[secondLen] = '\0';
    }
}

void initializeNcursesWindows()
{
    initscr();
    cbreak();
    noecho();

    int height, width;
    getmaxyx(stdscr, height, width);

    messageWindow = newwin(height - 3, width, 0, 0);
    userInputWindow = newwin(3, width, height - 3, 0);
    scrollok(messageWindow, TRUE);

    box(userInputWindow, 0, 0);
    mvwprintw(userInputWindow, 1, 1, "> ");
    wmove(userInputWindow, 1, 3);
    wrefresh(messageWindow);
    wrefresh(userInputWindow);
    nodelay(userInputWindow, TRUE);
}

int connectToServer(const char *serverIpAddress)
{
    struct sockaddr_in serverAddress;
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
    {
        return -1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, serverIpAddress, &serverAddress.sin_addr);

    if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        close(socketFileDescriptor);
        return -1;
    }
    // Get the client's IP address after connecting.
    getLocalIP(socketFileDescriptor, clientIP, sizeof(clientIP));
    return 0;
}

void *handleReceivedMessage(void *arg)
{
    (void)arg;
    while (1)
    {
        ssize_t numberOfBytesRead = read(socketFileDescriptor, receiveBuffer, MAX_PROTOL_MESSAGE_SIZE - 1);
        if (numberOfBytesRead > 0)
        {
            receiveBuffer[numberOfBytesRead] = '\0';

            // Check if the received message starts with our clientIP.
            if (strncmp(receiveBuffer, clientIP, strlen(clientIP)) == 0)
            {
                char displayMessage[MAX_PROTOL_MESSAGE_SIZE + 2]; // extra space for plus sign and null terminator
                snprintf(displayMessage, sizeof(displayMessage), "%s+", receiveBuffer);
                wprintw(messageWindow, "%s\n", displayMessage);
            }
            else
            {
                wprintw(messageWindow, "%s\n", receiveBuffer);
            }
            wrefresh(messageWindow);
        }
        else if (numberOfBytesRead == 0)
        {
            wprintw(messageWindow, "Server disconnected.\n");
            wrefresh(messageWindow);
            break;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            wprintw(messageWindow, "handleReceivedMessage() : Read error: %s\n", strerror(errno));
            wrefresh(messageWindow);
            break;
        }
        usleep(50000);
    }
    return NULL;
}

int startReceivingThread()
{
    pthread_t recvThread;
    if (pthread_create(&recvThread, NULL, handleReceivedMessage, NULL) != 0)
    {
        return -1;
    }
    pthread_detach(recvThread);
    return 0;
}

// Sends a protocol message to the server.
void sendProtocolMessage(const char *message)
{
    int len = strlen(message);
    if (write(socketFileDescriptor, message, len) < 0)
    {
        wprintw(messageWindow, "Failed to send message: %s\n", strerror(errno));
        wrefresh(messageWindow);
    }
}

void handleUserInput(char *clientName, char *clientIP)
{
    // clientIP is now available to send to the server or to be used to verify the broadcast.
    char sendBuffer[CLIENT_MAX_MSG_SIZE] = {0};
    int userInputIndex = 0;
    int currentCharacterAscii;

    while (1)
    {
        currentCharacterAscii = wgetch(userInputWindow);
        if (currentCharacterAscii == ERR)
        {
            usleep(50000);
            continue;
        }
        // On Enter with non-empty input.
        if (currentCharacterAscii == '\n' && userInputIndex > 0)
        {
            sendBuffer[userInputIndex] = '\0';
            int len = strlen(sendBuffer);
            char protocolMsg[MAX_PROTOL_MESSAGE_SIZE];
            char part1[CLIENT_MSG_PART_LENGTH + 1] = {"0"};
            char part2[CLIENT_MSG_PART_LENGTH + 1] = {"0"};

            // Hardcoded username "Chris"
            const char *username = clientName;
            if (len <= CLIENT_MSG_PART_LENGTH)
            {
                // Single message: MESSAGECOUNT 0.
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|0|%s", clientIP, username, sendBuffer);
                sendProtocolMessage(protocolMsg);
            }
            else
            {
                // Split the message into two parts.
                splitMessage(sendBuffer, part1, part2);
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|1|%s", clientIP, username, part1);
                sendProtocolMessage(protocolMsg);
                usleep(50000); // Small delay to help preserve order.
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|2|%s", clientIP, username, part2);
                sendProtocolMessage(protocolMsg);
            }
            // Clear the input (do not print the sent message in the chat window).
            memset(sendBuffer, 0, sizeof(sendBuffer));
            userInputIndex = 0;
            werase(userInputWindow);
            box(userInputWindow, 0, 0);
            mvwprintw(userInputWindow, 1, 1, "%s ", CLIENT_INPUT_MARKER);
            wmove(userInputWindow, 1, 3);
            wrefresh(userInputWindow);
        }
        else if (currentCharacterAscii != '\n')
        {
            if (userInputIndex < CLIENT_MAX_MSG_SIZE - 1)
            {
                sendBuffer[userInputIndex++] = currentCharacterAscii;
                sendBuffer[userInputIndex] = '\0';
                box(userInputWindow, 0, 0);
                mvwprintw(userInputWindow, 1, 1, "%s %s", CLIENT_INPUT_MARKER, sendBuffer);
                wmove(userInputWindow, 1, 3 + userInputIndex);
                wrefresh(userInputWindow);
            }
        }
    }
}

void cleanup()
{
    close(socketFileDescriptor);
    delwin(messageWindow);
    delwin(userInputWindow);
    endwin();
}

void check_host_name(int hostname)
{ // This function returns host name for local computer
    if (hostname == -1)
    {
        perror("gethostname");
        exit(1);
    }
}
void check_host_entry(struct hostent *hostentry)
{ // find host info from host name
    if (hostentry == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }
}
void IP_formatter(char *IPbuffer)
{ // convert IP string to dotted decimal format
    if (NULL == IPbuffer)
    {
        perror("inet_ntoa");
        exit(1);
    }
}

int main(int argc, char *argv[])
{

    /*

        TO DO: Need to check if when running the program NO arguments are provided!


    */


    // Used for getting clients own IP to send to server, or check IP on broadcast msg...
    char host[256];
    char *clientIP;
    struct hostent *host_entry;
    int hostname;
    hostname = gethostname(host, sizeof(host)); // find the host name
    check_host_name(hostname);
    host_entry = gethostbyname(host); // find host information
    check_host_entry(host_entry);
    clientIP = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0])); // Convert into IP string
    // printf("Current Host Name: %s\n", host);
    // printf("Host IP: %s\n", clientIP);
    // End of clients IP get***************************

    char userName[6] = {0};
    char serverIP[16] = {0};
    strcpy(userName, argv[1] + 5); // Only want to parse after the -user
    strcpy(serverIP, argv[2] + 7); // Only want to parse after the -server
    initializeNcursesWindows();

    if (connectToServer(serverIP) < 0)
    {
        wprintw(messageWindow, "Connect failed: %s\n", strerror(errno));
        wrefresh(messageWindow);
        cleanup();
        exit(EXIT_FAILURE);
    }

    wprintw(messageWindow, "Connected to server.\n");
    wrefresh(messageWindow);
    startReceivingThread();
    handleUserInput(userName, clientIP);
    cleanup();
    return 0;
}
