#include "../inc/chat-client.h"

// Global Socket
// int socketFileDescriptor;

// Ncurses Windows                                                                    
WINDOW *receivedMessagesWindow, 
*boxMsgWindow, 
*userInputWindow, 
*receivedTitle, 
*inputTitle;

// Buffer for received messages
char receiveBuffer[MAX_PROTOL_MESSAGE_SIZE];

// Store client address
char clientIP[256];

/*
* FUNCTION : getLocalIP
*
* DESCRIPTION : This function gets the clients local IP address for
*
* PARAMETERS : char *ipBuffer : Buffer to store the IP address.
*              size_t bufferSize : Size of the ipBuffer.
*
* RETURNS : void
*/
void getLocalIP(char *ipBuffer, size_t bufferSize)
{
    struct ifaddrs *interfaceAddress, *ifa;
    if (getifaddrs(&interfaceAddress) == -1)
    {
        strncpy(ipBuffer, "0.0.0.0", bufferSize);
        return;
    }

    // Iterate through available network interfaces
    for (ifa = interfaceAddress; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        // Only check for IPv4 addresses
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            // Skip the loopback interface
            if (strcmp(ifa->ifa_name, "lo") == 0)
            {
                continue;
            }

            struct sockaddr_in *socketAddress = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &socketAddress->sin_addr, ipBuffer, bufferSize);
            freeifaddrs(interfaceAddress);
            return;
        }
    }

    // Free the memory
    freeifaddrs(interfaceAddress);
    strncpy(ipBuffer, "0.0.0.0", bufferSize);
}

/*
* FUNCTION : splitMessage
*
* DESCRIPTION : This function splits a message into two parts if the message is longer than 40 characters
* It will try to do in a graceful way if possible
*
* PARAMETERS : const char *fullString : The full message to split.
*              char *firstPart : Buffer to store the first part of the message.
*              char *secondPart : Buffer to store the second part of the message.
*
* RETURNS : void
*/
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

/*
* FUNCTION : initializeNcursesWindows
*
* DESCRIPTION : This function sets up the ncurses windows to display incoming and outgoing messages nicely
*
* PARAMETERS : None
*
* RETURNS : void
*/
void initializeNcursesWindows(void)
{
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

/*
* FUNCTION : connectToServer
*
* DESCRIPTION : This function creates a socket, binds it to a port, and connects to the server
*
* PARAMETERS : const char *serverIpAddress : The server IP address as a string.
*              int *socketFileDescriptor : Pointer to an integer to store the created socket file descriptor.
*
* RETURNS : int : 0 on success, -1 on error.
*/
int connectToServer(const char *serverIpAddress, int *socketFileDescriptor)
{
    // Setup a struct to hold the socket info
    struct sockaddr_in serverAddress;

    // Setup the socket using IPv4
    *socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (*socketFileDescriptor < 0)
    {
        return -1;
    }

    // Setup a socketaddr_in struct for the client
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = 0;

    // Bind the client socket
    if (bind(*socketFileDescriptor, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0)
    {
        perror("bind failed");
        close(*socketFileDescriptor);
        return -1;
    }

    // memory to store the address information
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    // Convert the IP from the command line argument to an IP address
    int addressResult = inet_pton(AF_INET, serverIpAddress, &serverAddress.sin_addr);

    // If the address result is LESS than 0 (error)
    if (addressResult < 0)
    {
        // Error out if the address was invalid
        printf("INVALID ADDRESS.");
        return -1;
    }

    // Connect
    if (connect(*socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        close(*socketFileDescriptor);
        printf("ERROR CONNECTING TO SERVER!!\n\n");
        return -1;
    }

    // Get the client's IP address after connecting (for display purposes)
    getLocalIP(clientIP, sizeof(clientIP));
    return 0;
}

/*
* FUNCTION : handleReceivedMessage
*
* DESCRIPTION : This function runs in a separate thread to keep checking for messages from the chat server
* Messages are displayed using ncurses
*
* PARAMETERS : void *arg : Pointer to the socket file descriptor (cast to int *).
*
* RETURNS : void * : Always returns NULL.
*/
void *handleReceivedMessage(void *arg)
{
    // (void)arg; // Not using the argument
    int socketFileDescriptor = *((int *)arg);

    // Keep checking for messages in this loop
    while (1)
    {
        // Get current time
        time_t now;
        struct tm *timeInfo;

        // Read from the socket
        ssize_t numberOfBytesRead = read(socketFileDescriptor, receiveBuffer, MAX_PROTOL_MESSAGE_SIZE - 1);

        // If there are more than 0 bytes, there was a message
        if (numberOfBytesRead > 0)
        {
            receiveBuffer[numberOfBytesRead] = '\0';

            // Check if the received message starts with our clientIP
            if (strncmp(receiveBuffer, clientIP, strlen(clientIP)) == 0)
            {
                // Get the time
                time(&now);
                // Use local time
                timeInfo = localtime(&now);

                // Get hours, minutes, and seconds
                int hours = timeInfo->tm_hour;
                int minutes = timeInfo->tm_min;
                int seconds = timeInfo->tm_sec;
                char displayMessage[MAX_PROTOL_MESSAGE_SIZE + 20]; // extra space for plus sign and null terminator
                // If the message is from the client, change the >> to <<
                receiveBuffer[24] = '<';
                receiveBuffer[25] = '<';

                // Format the message
                snprintf(displayMessage, sizeof(displayMessage), "%s(%02d:%02d:%02d)", receiveBuffer, hours, minutes, seconds);

                // Print to the curses window
                wprintw(receivedMessagesWindow, "%s\n", displayMessage);
            }
            else
            {
                time(&now);
                timeInfo = localtime(&now);

                // Get hours, minutes, and seconds
                int hours = timeInfo->tm_hour;
                int minutes = timeInfo->tm_min;
                int seconds = timeInfo->tm_sec;
                char displayMessage[MAX_PROTOL_MESSAGE_SIZE + 20];
                snprintf(displayMessage, sizeof(displayMessage), "%s(%02d:%02d:%02d)", receiveBuffer, hours, minutes, seconds);
                wprintw(receivedMessagesWindow, "%s\n", displayMessage);
            }

            // Refresh curses window
            wrefresh(receivedMessagesWindow);

            // Move cursor to row 1, column 3 of the input window (after your input marker)
            wmove(userInputWindow, 1, 3);

            // Ensure the cursor is visible
            curs_set(1);

            // Refresh the input window to update the cursor position
            wrefresh(userInputWindow);
        }

        // If there were no bytes read, the server disconnected
        else if (numberOfBytesRead == 0)
        {
            wprintw(receivedMessagesWindow, "Server disconnected.\n");
            wrefresh(receivedMessagesWindow);
            break;
        }

        // Check for any errors
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            wprintw(receivedMessagesWindow, "handleReceivedMessage() : Read error: %s\n", strerror(errno));
            wrefresh(receivedMessagesWindow);
            break;
        }

        // Wait for a short time between messages
        usleep(50000);
    }

    // Return NULL because you have to return something
    return NULL;
}

/*
* FUNCTION : startReceivingThread
*
* DESCRIPTION : This function starts a thread that runs handleReceivedMessage to get messages from the server
*
* PARAMETERS : int *socketFileDescriptor : Pointer to the socket file descriptor.
*
* RETURNS : int : 0 on success, -1 on error.
*/
int startReceivingThread(int *socketFileDescriptor)
{
    // The thread
    pthread_t receivingThread;

    // Start the thread using the handleReceivedMessage() function
    if (pthread_create(&receivingThread, NULL, handleReceivedMessage, socketFileDescriptor) != 0)
    {
        return -1;
    }

    // All done
    pthread_detach(receivingThread);
    return 0;
}

/*
* FUNCTION : sendProtocolMessage
*
* DESCRIPTION : This function sends a formatted message to the server
*
* PARAMETERS : const char *message : The message to send.
*              int socketFileDescriptor : The socket file descriptor to use for sending.
*
* RETURNS : void
*/
void sendProtocolMessage(const char *message, int socketFileDescriptor)
{
    // Get the length of the message
    int len = strlen(message);

    // Write to the socket
    if (write(socketFileDescriptor, message, len) < 0)
    {
        // Error
        wprintw(receivedMessagesWindow, "Failed to send message: %s\n", strerror(errno));
        wrefresh(receivedMessagesWindow);
    }
}

/*
* FUNCTION : handleUserInput
*
* DESCRIPTION : This function handles user input from the ncurses window, and sends messages it to the server
*
* PARAMETERS : char *clientName : The name of the client.
*              char *clientIP : The IP address of the client.
*              int *socketFileDescriptor : Pointer to the socket file descriptor.
*
* RETURNS : void
*/
void handleUserInput(char *clientName, char *clientIP, int *socketFileDescriptor)
{
    // clientIP is now available to send to the server or to be used to verify the broadcast.
    char sendBuffer[CLIENT_MAX_MSG_SIZE] = {0};
    int userInputIndex = 0;
    int currentCharacterAscii;

    while (1)
    {
        // Get user input from the ncurses window userInputWindow
        currentCharacterAscii = wgetch(userInputWindow);

        // Check if there was an error getting the character
        if (currentCharacterAscii == ERR)
        {
            usleep(50000);
            continue;
        }

        // When the user presses enter, and there is something they typed
        if (currentCharacterAscii == '\n' && userInputIndex > 0)
        {
            sendBuffer[userInputIndex] = '\0';
            int bufferLength = strlen(sendBuffer);
            char protocolMsg[MAX_PROTOL_MESSAGE_SIZE];
            char messagePartOne[CLIENT_MSG_PART_LENGTH + 1] = {"0"};
            char messagePartTwo[CLIENT_MSG_PART_LENGTH + 1] = {"0"};

            // If the message is 40 characters or less
            if (bufferLength <= CLIENT_MSG_PART_LENGTH)
            {
                // Send a single message
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|0|%s", clientIP, clientName, sendBuffer);
                sendProtocolMessage(protocolMsg, *socketFileDescriptor);
            }

            // Otherwise split the message and send both parts
            else
            {
                // Split the message into two parts.
                splitMessage(sendBuffer, messagePartOne, messagePartTwo);
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|1|%s", clientIP, clientName, messagePartOne);
                sendProtocolMessage(protocolMsg, *socketFileDescriptor);

                // Small delay to give the first message time to arrive
                usleep(50000);
                snprintf(protocolMsg, sizeof(protocolMsg), "%s|%s|2|%s", clientIP, clientName, messagePartTwo);
                sendProtocolMessage(protocolMsg, *socketFileDescriptor);
            }

            // Clear the input
            memset(sendBuffer, 0, sizeof(sendBuffer));
            userInputIndex = 0; // reset index counter

            // Erase the user text from the window
            werase(userInputWindow);

            // TO ensure the box is drawn every time, I had issues getting the ncurses stuff to work how we needed
            box(userInputWindow, 0, 0);
            mvwprintw(userInputWindow, 1, 1, "%s ", CLIENT_INPUT_MARKER);

            // Move the cursor back to the input
            wmove(userInputWindow, 1, 3);
            wrefresh(userInputWindow);
        }

        // If anything other than enter was typed
        else if (currentCharacterAscii != '\n')
        {
            // Check if the input is less than the max size (-1 because the macro accounts for null terms)
            if (userInputIndex < CLIENT_MAX_MSG_SIZE - 1)
            {
                // Add the character to the buffer, increment the input index tracker
                sendBuffer[userInputIndex++] = currentCharacterAscii;
                sendBuffer[userInputIndex] = '\0'; // Set the next character to be a null terminator

                // TO ensure the box is drawn every time, I had issues getting the ncurses stuff to work how we needed
                box(userInputWindow, 0, 0);
                mvwprintw(userInputWindow, 1, 1, "%s %s", CLIENT_INPUT_MARKER, sendBuffer);
                wmove(userInputWindow, 1, 3 + userInputIndex);
                wrefresh(userInputWindow);
            }
        }
    }
}

/*
* FUNCTION : cleanup
*
* DESCRIPTION : Closes all the ncurses windows and closes the socket
*
* PARAMETERS : int *socketFileDescriptor : Pointer to the socket file descriptor to close.
*
* RETURNS : void
*/
void cleanup(int *socketFileDescriptor)
{
    // Close the socket, delete the windows
    close(*socketFileDescriptor);
    delwin(receivedMessagesWindow);
    delwin(userInputWindow);
    delwin(inputTitle);
    delwin(receivedTitle);
    endwin();
}

/*
* FUNCTION : getUserName
*
* DESCRIPTION : This function parses the user name from the command line arguments
*
* PARAMETERS : char *userArg : The argument string containing the user name (prefixed with "-user").
*              char *userName : Buffer to store the extracted user name.
*
* RETURNS : int : 1 on success, -1 on error.
*/
int getUserName(char *userArg, char* userName)
{
    // Check for username switch
    if (strstr(userArg, "-user") != NULL)
    {
        // Parse and set username var if if not blank past the switch -user
        userArg += strlen("-user"); // iterate past the -user

        // Check to make sure strlen is valid 5 chars
        if (strlen(userArg) > 5)
        {
            printf("User name exceedes the 5 character limit!\n");
            printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name.\n");
            return -1;
        }

        if (strcmp(userArg, "") != 0)
        {
            strcpy(userName, userArg);
            return 1;
        }
        else
        {
            printf("No user name attached to the -user switch!\n");
            printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name.\n");
            return -1;
        }
    }
    else // No -user switch
    {
        printf("No -user switch!\n");
        printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name.\n");
        return -1;
    }
}

/*
* FUNCTION : getServerAddress
*
* DESCRIPTION : This function parses the server address from the command line arguments
*
* PARAMETERS : char *serverArgument : The argument string containing the server address (prefixed with "-server").
*              char *serverAddress : Buffer to store the extracted server address.
*
* RETURNS : int : 1 on success, or a negative error code on error.
*/
int getServerAddress(char *serverArgument, char *serverAddress)
{
    if (strstr(serverArgument, "-server") != NULL)
    {
        // Parse and set server name var if if not blank past the switch -server
        serverArgument += strlen("-server"); // iterate past the -server
        if (strcmp(serverArgument, "") != 0)
        {
            // check for ip
            int part1, part2, part3, part4, result;

            // Check to see if the argument is an ip address or a server name
            result = sscanf(serverArgument, "%d.%d.%d.%d", &part1, &part2, &part3, &part4);

            if (result == 4)
            {
                // We have an Ip address
                if (part1 >= 0 && part1 < 256 && part2 >= 0 && part2 < 256 && part3 >= 0 && part3 < 256 && part4 >= 0 && part4 < 256)
                {
                    // Ip address is valid
                    strcpy(serverAddress, serverArgument);
                }
                else
                {
                    // error invalid Ip
                    printf("Server switch Ip Address is INVALID!\n");
                    printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name or Ip address.\n");
                    return -4;
                }
            }
            else if (result == 0)
            {
                // Else we have a server name
                strcpy(serverAddress, serverArgument);
                return 1;
            }
        }
        else
        {
            printf("No server name attached to the -server switch!\n");
            printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name or Ip address.\n");
            return -2;
        }
    }
    else
    {
        printf("No -server switch!\n");
        printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name or Ip address.\n");
        return -3;
    }
}


int main(int argc, char *argv[])
{

    char userName[6];
    char serverName[256] = "Ip address used";
    int socketFileDescriptor;
    // char serverIp[17] = "Server name used";

    // Check if arg count is valid
    if (argc != 3)
    {
        printf("Not Enough Arguments\n");
        printf("Usage: <arg1> <arg2> <arg3>\nWhere arg1 is the exe, arg2 is the user, arg3 is the server name.\n");
        exit(EXIT_FAILURE);
    }

    // Get the user name from the arguments
    int userNameResult = getUserName(argv[1], userName);
    if(userNameResult < 0)
    {
        // Exit with error
        exit(EXIT_FAILURE);
    }

    // Get the server address from the arguments
    int serverAddressResult = getServerAddress(argv[2], serverName);
    if(serverAddressResult < 0)
    {
        // Exit with error
        exit(EXIT_FAILURE);
    }

    // Attempt to connect to the server
    if (connectToServer(serverName, &socketFileDescriptor) < 0)
    {
        // wprintw(receivedMessagesWindow, "Connect failed: %s\n", strerror(errno));
        // wrefresh(receivedMessagesWindow);
        cleanup(&socketFileDescriptor);
        exit(EXIT_FAILURE);
    }

    // Initialize the ncurses windows
    initializeNcursesWindows();
    // THIS WAS CHANGED: Use 'host' from gethostname() instead of hostDetails->h_name.
    // wprintw(receivedMessagesWindow, "Host Name: %s\n", host);
    // wprintw(receivedMessagesWindow, "CLIENT IP: %s\n", clientIP);
    // wprintw(receivedMessagesWindow, "Server : %s\n", serverName);
    // wrefresh(receivedMessagesWindow);
    startReceivingThread(&socketFileDescriptor);
    handleUserInput(userName, clientIP, &socketFileDescriptor);
    cleanup(&socketFileDescriptor);
    return 0;
}
