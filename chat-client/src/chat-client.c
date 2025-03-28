// CHANGED THIS: Added necessary POSIX and networking headers before any other includes.
#define _POSIX_C_SOURCE 200112L           // CHANGED THIS: Enable POSIX definitions
#include <sys/types.h>                    // CHANGED THIS: Required for sockets
#include <sys/socket.h>                   // CHANGED THIS: Required for sockets
#include <netdb.h>                        // CHANGED THIS: Provides definition for struct addrinfo and getaddrinfo
#include <arpa/inet.h>                    // CHANGED THIS: For inet_pton and inet_ntop
#include <unistd.h>                       // CHANGED THIS: For close()
#include <stdio.h>                        // CHANGED THIS: For printf()
#include <stdlib.h>                       // CHANGED THIS: For exit()
#include <string.h>                       // CHANGED THIS: For memset(), strcpy(), etc.

#include "../inc/chat-client.h"

/*
 * chat-client.c
 *
 * Updated to use ncurses for I/O and to send messages using a custom protocol:
 *   CLIENTIPADDRESS|CLIENTUSERNAME|MESSAGECOUNT|"Message text"
 *
 * If a message exceeds 40 characters, it is split into two parts:
 *   MESSAGECOUNT 1 for the first part, and MESSAGECOUNT 2 for the second.
 * The server then parses this protocol message and broadcasts
 * the formatted message (e.g., 127.0.0.1 [Chris] MESSAGE)
 * to every connected client (including the sender).
 *
 * When receiving a message, the client checks if the IP in the message
 * matches its own. If so, it appends a plus sign (+) to the end of the message.
 */

int socketFileDescriptor;                                                                     // Global socket descriptor
WINDOW *receivedMessagesWindow, *boxMsgWindow, *userInputWindow, *receivedTitle, *inputTitle; // ncurses windows for chat display and input
char receiveBuffer[MAX_PROTOL_MESSAGE_SIZE];                                                  // Buffer for incoming messages

char clientIP[INET_ADDRSTRLEN]; // Stores the client's IP address

// Function prototypes
// These belong in the chat-client.h file

// SOME FUNCTION PROTOTYPES ARE MISSING!
void initializeNcursesWindows(void);
int connectToServer(const char *serverIpAddress);
void *handleReceivedMessage(void *arg);
int startReceivingThread(void);
void handleUserInput(char *userName, char *clientIP);
void cleanup(void);
void getLocalIP(int socketDescriptor, char *ipBuffer, size_t bufferSize);
void splitMessage(const char *fullString, char *firstPart, char *secondPart);

void checkHostName(int hostname);
void checkHostEntryDetails(struct hostent *hostentry);
void ipAddressFormatter(char *IPbuffer);

// THIS WAS CHANGED: Modified getLocalIP() to use a dummy UDP socket to get the external IP.
void getLocalIP(int socketDescriptor, char *ipBuffer, size_t bufferSize)
{
    (void) socketDescriptor; // Unused in the new implementation.
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        strncpy(ipBuffer, "0.0.0.0", bufferSize);
        return;
    }
    
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");  // Using Google DNS to determine external IP.
    serv.sin_port = htons(53);
    
    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        close(sock);
        strncpy(ipBuffer, "0.0.0.0", bufferSize);
        return;
    }
    
    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) < 0)
    {
        close(sock);
        strncpy(ipBuffer, "0.0.0.0", bufferSize);
        return;
    }
    
    inet_ntop(AF_INET, &local.sin_addr, ipBuffer, bufferSize);
    close(sock);
}

// Splitting function based on the provided algorithm
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

    // Get the MINIMUM split location (-40)
    int minSplit = fullStringLength - CLIENT_MSG_PART_LENGTH;

    // Max split assigned 40 to start
    int maxSplit = CLIENT_MSG_PART_LENGTH;

    // Divide to get possible mid-point of full message
    int midPoint = fullStringLength / 2;

    // PLaceholder for split index
    int splitIndex = -1;

    // Iterate over the the message to find where to potentially split it (checking for a space in the middle or close to it)
    for (
        // Start the offset at 0
        int splitOffset = 0;
        // WHILE the split offset is LESS than or EQUAL to the max (string length - 40)
        splitOffset <= (maxSplit - minSplit);
        // Increment the split offset to keep checking
        splitOffset++)
    {
        // Index of the lower half of the message by subtracting the midpoint from the split offset
        int lowerHalfMessage = midPoint - splitOffset;

        if (
            // If the lower half of the message split is GREATER or EQUAL to minsplit
            lowerHalfMessage >= minSplit &&
            // AND the lower half is LESS THAN or EQUAL to the maxsplit
            lowerHalfMessage <= maxSplit &&
            // And the index of the full string is a SPACE (proper split!)
            fullString[lowerHalfMessage] == ' ')
        {
            // Assign the split index to the current index of the space!
            splitIndex = lowerHalfMessage;
            break;
        }

        // Set the upper half to be the midpoint + the split offset (if the above IF was not used)
        // This attempts to split at the best possible location in a not great case
        // Calculate where the upper half of the message is using the mid point and the split offset
        int upperHalfMessage = midPoint + splitOffset;

        if (
            // Is the upper hald of the message is GREATER or EQUAL to the MINIMUM split index
            upperHalfMessage >= minSplit &&

            // AND the upper half is LESS than or EQUAL to the MAXIMUM split index
            upperHalfMessage <= maxSplit &&

            // AND the character is a SPACE!
            fullString[upperHalfMessage] == ' ')
        {
            // Set the split index to be the location of the best possible split location
            splitIndex = upperHalfMessage;
            break;
        }
    }

    // If the split index is -1, split it right down the middle (no space, no best split, just cut it in half)
    if (splitIndex == -1)
    {
        splitIndex = midPoint;
    }

    // If there was a space in the middle of the message (easiest case!), split it there!
    if (fullString[splitIndex] == ' ')
    {
        // Copy the first part of the split using the split index
        strncpy(firstPart, fullString, splitIndex);

        // Set the last character of the first part to be a null term (proper string yo!)
        firstPart[splitIndex] = '\0';

        // Get the second half of the string using the FULL length and subtracting the location of the split +1 to offset
        // the split
        int secondHalfLength = fullStringLength - (splitIndex + 1);
        if (secondHalfLength > CLIENT_MSG_PART_LENGTH)
        {
            secondHalfLength = CLIENT_MSG_PART_LENGTH;
        }

        // Copy the second part and add a null terminator
        strncpy(secondPart, fullString + splitIndex + 1, secondHalfLength);
        secondPart[secondHalfLength] = '\0';
    }

    // Otherwise, just cut the whole thing in half, most likely it was a big word in the middle of a large message
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

void initializeNcursesWindows(void)
{

    /*
    Declared windows:
    *receivedMessagesWindow, *boxMsgWindow, *userInputWindow, *receivedTitle, *inputTitle;
    */
    initscr();
    cbreak();
    noecho();

    // Get the height/width details from the terminal
    int height, width;
    getmaxyx(stdscr, height, width);

    // Allow coloUrs to be used
    start_color();

    // Test pair of colours
    init_pair(1, COLOR_BLACK, COLOR_WHITE); // Title bar for messages received
    init_pair(2, COLOR_BLACK, COLOR_YELLOW); // Received message area
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Title bar for the user input
    init_pair(4, COLOR_BLACK, COLOR_WHITE); // Actual user input areavvv

    /*
    Received Messages and Title Area
    */
    receivedTitle = newwin(3, width, 0, 0);
    wbkgd(receivedTitle, COLOR_PAIR(1));

    boxMsgWindow = newwin(13, width, 3, 0);
    receivedMessagesWindow = newwin(11, width - 2, 4, 1);
    wbkgd(receivedMessagesWindow, COLOR_PAIR(2));
    leaveok(receivedMessagesWindow, TRUE);
    scrollok(receivedMessagesWindow, TRUE);
    box(boxMsgWindow, 0, 0);
    box(receivedTitle, 0, 0);
    mvwprintw(receivedTitle, 1, 1, CHAT_TITLE);
    wrefresh(receivedTitle);
    wrefresh(boxMsgWindow);
    wrefresh(receivedMessagesWindow);

    /*
    Input window and TITLE area
    */
    inputTitle = newwin(3, width, height - 6, 0);
    wbkgd(inputTitle, COLOR_PAIR(3));
    userInputWindow = newwin(3, width, height - 3, 0);
    wbkgd(userInputWindow, COLOR_PAIR(4));
    leaveok(userInputWindow, FALSE);
    box(inputTitle, 0, 0);
    box(userInputWindow, 0, 0);
    mvwprintw(inputTitle, 1, 1, "============= USER INPUT =============");
    mvwprintw(userInputWindow, 1, 1, "> ");
    wrefresh(userInputWindow);
    wrefresh(inputTitle);
    nodelay(userInputWindow, TRUE);
    wmove(userInputWindow, 1, 3);
    curs_set(1);
    wrefresh(userInputWindow);
}

int connectToServer(const char *serverIpAddress)
{
    // Setup a struct to hold the socket info
    struct sockaddr_in serverAddress;

    // Setup the socket using IPv4
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
    {
        return -1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    // CHANGED THIS: First attempt to convert as an IP address.
    int addressResult = inet_pton(AF_INET, serverIpAddress, &serverAddress.sin_addr);
    if (addressResult == 0) {
        // CHANGED THIS: hostname provided, resolve with getaddrinfo
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        int err = getaddrinfo(serverIpAddress, NULL, &hints, &res);
        if (err != 0 || res == NULL)
        {
            printf("Hostname resolution error for %s: %s\n", serverIpAddress, gai_strerror(err));
            return -1;
        }
        struct sockaddr_in *resolved = (struct sockaddr_in *)res->ai_addr;
        serverAddress.sin_addr = resolved->sin_addr;
        freeaddrinfo(res);
    }
    else if (addressResult < 0)
    {
        // Error out if the conversion fails with an error.
        printf("INVALID ADDRESS.\n");
        return -1;
    }

    if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        close(socketFileDescriptor);
        printf("ERROR CONNECTING TO SERVER!!\n\n");
        return -1;
    }

    // Get the client's IP address after connecting
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

            if (strncmp(receiveBuffer, clientIP, strlen(clientIP)) == 0)
            {
                char displayMessage[MAX_PROTOL_MESSAGE_SIZE + 2];
                snprintf(displayMessage, sizeof(displayMessage), "%s+", receiveBuffer);
                wprintw(receivedMessagesWindow, "%s\n", displayMessage);
            }
            else
            {
                wprintw(receivedMessagesWindow, "%s\n", receiveBuffer);
            }
            wrefresh(receivedMessagesWindow);
            wmove(userInputWindow, 1, 3);
            curs_set(1);
            wrefresh(userInputWindow);
        }
        else if (numberOfBytesRead == 0)
        {
            wprintw(receivedMessagesWindow, "Server disconnected.\n");
            wrefresh(receivedMessagesWindow);
            break;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            wprintw(receivedMessagesWindow, "handleReceivedMessage() : Read error: %s\n", strerror(errno));
            wrefresh(receivedMessagesWindow);
            break;
        }
        usleep(50000);
    }
    return NULL;
}

int startReceivingThread()
{
    pthread_t receivingThread;
    if (pthread_create(&receivingThread, NULL, handleReceivedMessage, NULL) != 0)
    {
        return -1;
    }
    pthread_detach(receivingThread);
    return 0;
}

void sendProtocolMessage(const char *message)
{
    int len = strlen(message);
    if (write(socketFileDescriptor, message, len) < 0)
    {
        wprintw(receivedMessagesWindow, "Failed to send message: %s\n", strerror(errno));
        wrefresh(receivedMessagesWindow);
    }
}

void handleUserInput(char *clientName, char *clientIP)
{
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
        if (currentCharacterAscii == '\n' && userInputIndex > 0)
        {
            sendBuffer[userInputIndex] = '\0';
            int len = strlen(sendBuffer);
            char protocolMsg[MAX_PROTOL_MESSAGE_SIZE];
            char messagePartOne[CLIENT_MSG_PART_LENGTH + 1] = {"0"};
            char messagePartTwo[CLIENT_MSG_PART_LENGTH + 1] = {"0"};
            const char *username = clientName;
            if (len <= CLIENT_MSG_PART_LENGTH)
            {
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|0|%s", clientIP, clientName, sendBuffer);
                sendProtocolMessage(protocolMsg);
            }
            else
            {
                splitMessage(sendBuffer, messagePartOne, messagePartTwo);
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|1|%s", clientIP, clientName, messagePartOne);
                sendProtocolMessage(protocolMsg);
                usleep(50000);
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|2|%s", clientIP, clientName, messagePartTwo);
                sendProtocolMessage(protocolMsg);
            }
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
    delwin(receivedMessagesWindow);
    delwin(userInputWindow);
    endwin();
}

void checkHostName(int hostname)
{
    if (hostname == -1)
    {
        perror("gethostname");
        exit(1);
    }
}
void checkHostEntryDetails(struct hostent *hostentry)
{
    if (hostentry == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }
}
void ipAddressFormatter(char *IPbuffer)
{
    if (NULL == IPbuffer)
    {
        perror("inet_ntoa");
        exit(1);
    }
}
void updateUserInputWindow(WINDOW *inputWin, const char *currentBuffer, int userInputIndex)
{
    werase(inputWin);
    box(inputWin, 0, 0);
    mvwprintw(inputWin, 1, 1, "%s %s", CLIENT_INPUT_MARKER, currentBuffer);
    wmove(inputWin, 1, 3 + userInputIndex);
    wrefresh(inputWin);
}

int main(int argc, char *argv[])
{
    char userName[6];
    char serverName[256] = "Ip address used";

    if (argc != 3)
    {
        printf("Not Enough Arguments\n");
        printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name.\n");
        return -1;
    }

    if (strstr(argv[1], "-user") != NULL)
    {
        argv[1] += strlen("-user");
        if (strlen(argv[1]) > 5)
        {
            printf("User name exceedes the 5 character limit!\n");
            return -5;
        }
        if (strcmp(argv[1], "") != 0)
        {
            strcpy(userName, argv[1]);
        }
        else
        {
            printf("No user name attached to the -user switch!\n");
            return -2;
        }
    }
    else
    {
        printf("No -user switch!\n");
        return -3;
    }

    if (strstr(argv[2], "-server") != NULL)
    {
        argv[2] += strlen("-server");
        if (strcmp(argv[2], "") != 0)
        {
            int part1, part2, part3, part4, result;
            result = sscanf(argv[2], "%d.%d.%d.%d", &part1, &part2, &part3, &part4);
            if (result == 4)
            {
                if (part1 >= 0 && part1 < 256 && part2 >= 0 && part2 < 256 && part3 >= 0 && part3 < 256 && part4 >= 0 && part4 < 256)
                {
                    strcpy(serverName, argv[2]);
                }
                else
                {
                    printf("Server switch Ip Address is INVALID!\n");
                    return -4;
                }
            }
            else if (result == 0)
            {
                strcpy(serverName, argv[2]);
            }
        }
        else
        {
            printf("No server name attached to the -server switch!\n");
            return -2;
        }
    }
    else
    {
        printf("No -server switch!\n");
        return -3;
    }

    printf("User Name: %s\n", userName);
    printf("Server Name: %s\n", serverName);
    printf("Socket Ip: %s\n", serverName);

    char host[256];
    gethostname(host, sizeof(host));

    initializeNcursesWindows();

    if (connectToServer(serverName) < 0)
    {
        wprintw(receivedMessagesWindow, "Connect failed: %s\n", strerror(errno));
        wrefresh(receivedMessagesWindow);
        cleanup();
        exit(EXIT_FAILURE);
    }

    wprintw(receivedMessagesWindow, "Host Name: %s\n", host);
    wprintw(receivedMessagesWindow, "CLIENT IP: %s\n", clientIP);
    wprintw(receivedMessagesWindow, "Server : %s\n", serverName);
    wrefresh(receivedMessagesWindow);
    startReceivingThread();
    handleUserInput(userName, clientIP);
    cleanup();
    return 0;
}
