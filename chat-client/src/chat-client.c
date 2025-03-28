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

#include "../inc/chat-client.h"

// some of these belong in the common.h file
// #define SERVER_PORT 8888      // Port number of chat server
// #define MAX_MESSAGE_SIZE 81   // Maximum length of a user-typed message
// #define MAX_PROTOCOL_SIZE 128 // Buffer size for protocol messages
// #define MAX_PART_LEN 40       // Maximum characters per message part

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

// Get the local IP address from the connected socket
void getLocalIP(int socketDescriptor, char *ipBuffer, size_t bufferSize)
{
    struct sockaddr_in address;
    socklen_t addressLength = sizeof(address);
    if (getsockname(socketDescriptor, (struct sockaddr *)&address, &addressLength) == 0)
    {
        inet_ntop(AF_INET, &address.sin_addr, ipBuffer, bufferSize);
    }
    else
    {
        strncpy(ipBuffer, "0.0.0.0", bufferSize);
    }
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
    // Create the title for the received messages
    receivedTitle = newwin(3, width, 0, 0);
    wbkgd(receivedTitle, COLOR_PAIR(1));

    // Box for the message window
    boxMsgWindow = newwin(13, width, 3, 0);

    // Window to display the received messages
    receivedMessagesWindow = newwin(11, width - 2, 4, 1);
    wbkgd(receivedMessagesWindow, COLOR_PAIR(2));

    // Dont touch the cursor in the received messages window
    leaveok(receivedMessagesWindow, TRUE);

    // This ensures that when the 10 message limit is reached in the received messages window
    // That the window will scroll to show the new messages (without this they will NOT display properly!)
    scrollok(receivedMessagesWindow, TRUE);

    // Box to put the received messages inside of
    box(boxMsgWindow, 0, 0);

    // Box for the received TITLE
    box(receivedTitle, 0, 0);

    // Print title message for received messages title
    mvwprintw(receivedTitle, 1, 1, CHAT_TITLE);

    // Refresh Received Messages display
    wrefresh(receivedTitle);
    wrefresh(boxMsgWindow);
    wrefresh(receivedMessagesWindow);

    /*
    Input window and TITLE area
    */
    // Create the title window for the user input
    inputTitle = newwin(3, width, height - 6, 0);
    wbkgd(inputTitle, COLOR_PAIR(3));

    // Create the user input window
    userInputWindow = newwin(3, width, height - 3, 0);
    wbkgd(userInputWindow, COLOR_PAIR(4));

    // Keep the cursor in the user input area
    leaveok(userInputWindow, FALSE);

    // Box for the input title
    box(inputTitle, 0, 0);

    // Box for the user input
    box(userInputWindow, 0, 0);

    // Refresh Input title to display text
    mvwprintw(inputTitle, 1, 1, "============= USER INPUT =============");

    // Draw the cursor when initializing, because ncurses works so well
    mvwprintw(userInputWindow, 1, 1, "> ");
    wrefresh(userInputWindow);
    wrefresh(inputTitle);
    nodelay(userInputWindow, TRUE);

    // Move cursor to row 1, column 3 of the input window
    wmove(userInputWindow, 1, 3);
    curs_set(1);               // Ensure the cursor is visible
    wrefresh(userInputWindow); // Refresh the input window to update the cursor position
}

int connectToServer(const char *serverIpAddress)
{
    // Setup a struct to hold the socket info
    struct sockaddr_in serverAddress;

    // Setup the socket using IPv4
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);

    // If the socket didnt work
    if (socketFileDescriptor < 0)
    {
        return -1;
    }

    // Allocate memory to store the address information
    memset(&serverAddress, 0, sizeof(serverAddress));
    // Setup address details
    serverAddress.sin_family = AF_INET;
    // Setup the port
    serverAddress.sin_port = htons(SERVER_PORT);

    // Convert the IP from the command line argument to an IP address
    int addressResult = inet_pton(AF_INET, serverIpAddress, &serverAddress.sin_addr);

    if (addressResult < 0)
    {
        // Error out if the address was invalid
        printf("INVALID ADDRESS.");
        return -1;
    }

    if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        close(socketFileDescriptor);
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

            // Check if the received message starts with our clientIP
            if (strncmp(receiveBuffer, clientIP, strlen(clientIP)) == 0)
            {
                char displayMessage[MAX_PROTOL_MESSAGE_SIZE + 2]; // extra space for plus sign and null terminator
                snprintf(displayMessage, sizeof(displayMessage), "%s+", receiveBuffer);
                wprintw(receivedMessagesWindow, "%s\n", displayMessage);
            }
            else
            {
                wprintw(receivedMessagesWindow, "%s\n", receiveBuffer);
            }
            wrefresh(receivedMessagesWindow);
            wmove(userInputWindow, 1, 3); // Move cursor to row 1, column 3 of the input window (after your input marker)
            curs_set(1);                  // Ensure the cursor is visible
            wrefresh(userInputWindow);    // Refresh the input window to update the cursor position
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

// Sends a protocol message to the server.
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
            char messagePartOne[CLIENT_MSG_PART_LENGTH + 1] = {"0"};
            char messagePartTwo[CLIENT_MSG_PART_LENGTH + 1] = {"0"};

            // Hardcoded username "Chris"
            const char *username = clientName;
            if (len <= CLIENT_MSG_PART_LENGTH)
            {
                // Single message: MESSAGECOUNT 0.
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|0|%s", clientIP, clientName, sendBuffer);
                sendProtocolMessage(protocolMsg);
            }

            else
            {
                // Split the message into two parts.
                splitMessage(sendBuffer, messagePartOne, messagePartTwo);
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|1|%s", clientIP, clientName, messagePartOne);
                sendProtocolMessage(protocolMsg);
                usleep(50000); // Small delay to help preserve order.
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|2|%s", clientIP, clientName, messagePartTwo);
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
    delwin(receivedMessagesWindow);
    delwin(userInputWindow);
    endwin();
}

void checkHostName(int hostname)
{ // This function returns host name for local computer
    if (hostname == -1)
    {
        perror("gethostname");
        exit(1);
    }
}
void checkHostEntryDetails(struct hostent *hostentry)
{ // find host info from host name
    if (hostentry == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }
}

void ipAddressFormatter(char *IPbuffer)
{ // convert IP string to dotted decimal format
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
    // Print the input marker and current user input.
    mvwprintw(inputWin, 1, 1, "%s %s", CLIENT_INPUT_MARKER, currentBuffer);
    // Move the cursor to the appropriate position (adjust the coordinates as needed).
    wmove(inputWin, 1, 3 + userInputIndex);
    wrefresh(inputWin);
}

int main(int argc, char *argv[])
{

    // double check user name size
    char userName[6] = {0};
    char serverIP[16] = {0};

    char errorMessage[256];
    int validArguments = 1;

    if (argc != 3)
    {
        validArguments = 0;
        strcpy(errorMessage, TOO_FEW_ARGUMENTS);
    }

    else
    {
        // Check if the first argument is over 10 characters
        if (strlen(argv[1]) > 10)
        {
            printf("User name too long!\n");
            return -1;
        }
        strcpy(userName, argv[1] + 5); // Only want to parse after the -user
        strcpy(serverIP, argv[2] + 7); // Only want to parse after the -server
    }
    /*

        TO DO: Need to check if when running the program NO arguments are provided!


    */

    // If there are valid arguments
    if (validArguments)
    {
        // Used for getting clients own IP to send to server, or check IP on broadcast msg...
        char host[256];
        char *clientIP;

        struct hostent *hostDetails;
        int hostname;
        hostname = gethostname(host, sizeof(host)); // find the host name

        // Rework this function to check for the host name
        // checkHostName(hostname);

        hostDetails = gethostbyname(host); // find host information
        // checkHostEntryDetails(hostDetails);

        clientIP = inet_ntoa(*((struct in_addr *)hostDetails->h_addr_list[0])); // Convert into IP string
        // printf("Current Host Name: %s\n", host);
        // printf("Host IP: %s\n", clientIP);
        // End of clients IP get***************************

        initializeNcursesWindows();

        if (connectToServer(serverIP) < 0)
        {
            wprintw(receivedMessagesWindow, "Connect failed: %s\n", strerror(errno));
            wrefresh(receivedMessagesWindow);
            cleanup();
            exit(EXIT_FAILURE);
        }

        // wprintw(receivedMessagesWindow, "Connected to server.\n");

        // Display host details from struct
        // wprintw(receivedMessagesWindow, "DEBUG: IP: %s\n", clientIP);

        // This is what will need to be used when to get the server HOST NAME (non-ip)
        wprintw(receivedMessagesWindow, "Host Name: %s\n", hostDetails->h_name);

        wrefresh(receivedMessagesWindow);
        startReceivingThread();
        handleUserInput(userName, clientIP);
        cleanup();
        return 0;
    }

    else
    {
        printf("ERROR: %s", errorMessage);
        return -1;
    }
}
