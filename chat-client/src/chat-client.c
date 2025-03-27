#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>

#define SERVER_PORT 8888    // Port number of chat server
#define MAX_MESSAGE_SIZE 81 // Maximum length of a chat message

int socketFileDescriptor;                // Global socket descriptor
WINDOW *messageWindow, *userInputWindow; // ncurses windows for chat display and input
char receiveBuffer[MAX_MESSAGE_SIZE];    // Buffer for incoming messages

// Function prototypes
void initializeNcursesWindows(void);
int connectToServer(const char *serverIpAddress);
void *handleReceivedMessage();
int startReceivingThread(void);
void handleUserInput(char* userName, char* clientIP);
void cleanup(void);

/**
 * Initialize ncurses, create windows for messages and user input,
 * enable scrolling on message window, and set input to non-blocking.
 */
void initializeNcursesWindows()
{
    initscr(); // Start ncurses mode
    cbreak();  // Disable line buffering
    noecho();  // Don't echo typed characters

    int height, width;
    getmaxyx(stdscr, height, width);

    messageWindow = newwin(height - 3, width, 0, 0);
    userInputWindow = newwin(3, width, height - 3, 0);
    scrollok(messageWindow, TRUE); // Allow scrolling when new text is added

    box(userInputWindow, 0, 0);             // Draw border around input window
    mvwprintw(userInputWindow, 1, 1, "> "); // Print prompt
    wmove(userInputWindow, 1, 3);
    wrefresh(messageWindow);
    wrefresh(userInputWindow);
    nodelay(userInputWindow, TRUE); // Make wgetch non-blocking
}

/**
 * Create a TCP socket and connect to the specified IP on SERVER_PORT.
 * Returns 0 on success, -1 on failure.
 */
int connectToServer(const char *serverIpAddress)
{
    struct sockaddr_in serverAddress;
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
    {
        return -1;
    }

    // Allocate some memory for the serverAddress socket
    memset(&serverAddress, 0, sizeof(serverAddress));

    // Setup to use IPV4
    serverAddress.sin_family = AF_INET;

    // Use port from SERVER_PORT
    serverAddress.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET, serverIpAddress, &serverAddress.sin_addr);

    if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        close(socketFileDescriptor);
        return -1;
    }
    return 0;
}

/**
 * Thread function: blocks on read() to receive messages from server.
 * Prints incoming text to messageWindow until connection closes or error occurs.
 */
void *handleReceivedMessage()
{
    while (1)
    {
        // This MAX_MESSAGE_SIZE will need to be changed to a max LINE size
        // To account for the protocol characters AND the message itself
        ssize_t numberOfBytesRead = read(socketFileDescriptor, receiveBuffer, MAX_MESSAGE_SIZE + 16);

        if (numberOfBytesRead > 0)
        {
            // The buffer to store the string read from the socket
            receiveBuffer[numberOfBytesRead] = '\0';

            // Parse out the message from the raw protocol message, then
            // snprintf the message together with the extracted user name
            // and ip address and concatenate the date to the end of the message to display it

            // Get readt to print the buffer to the message window in ncurses
            wprintw(messageWindow, "%s\n", receiveBuffer);

            // Refresh the window to display the changes
            wrefresh(messageWindow);
        }

        // If the number of bytes read == 0, the server has disconnected
        else if (numberOfBytesRead == 0)
        {
            // Server closed connection
            wprintw(messageWindow, "Server disconnected.\n");
            wrefresh(messageWindow);
            break;
        }

        // Check for any errors from the socket
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // Unexpected read error
            wprintw(messageWindow, "handleReceivedMessage() : Read error: %s\n", strerror(errno));
            wrefresh(messageWindow);
            break;
        }
        usleep(50000); // Slight delay
    }
    return NULL;
}

/**
 * Launch the handleReceivedMessage in a detached thread.
 * Returns 0 on success, -1 on thread creation failure.
 */
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

/**
 * Main input loop: reads keystrokes, builds a message buffer,
 * sends complete lines to server on Enter, and updates UI.
 */
void handleUserInput(char* userName, char* clientIP)
{
    char sendBuffer[MAX_MESSAGE_SIZE] = {0};
    char clientProtocol[65] = {0};

    int userInputIndex = 0;    // For tracking WHERE to put the character the user types in the sendBuffer
    int currentCharacterAscii; // Storage for the current character ascii value, or error return from wgetch()

    while (1)
    {
        currentCharacterAscii = wgetch(userInputWindow);

        // If there was an error getting a character from the user
        // Keep getting input
        if (currentCharacterAscii == ERR)
        {
            usleep(50000);
            continue;
        }

        // If the user pressed enter and the input is GREATER than 0 (the user typed a character)
        if (currentCharacterAscii == '\n' && userInputIndex > 0)
        {
            //Prep protocol with IP and Name
            strcat(clientProtocol, clientIP);
            strcat(clientProtocol, "|");
            strcat(clientProtocol, userName);
            strcat(clientProtocol, "|");
            strcat(clientProtocol, "0");
            strcat(clientProtocol, "|");


            // Send when Enter pressed
            strcat(clientProtocol, sendBuffer);


            wprintw(messageWindow, "%s\n", clientProtocol);


            //write(socketFileDescriptor, sendBuffer, userInputIndex);
            write(socketFileDescriptor, clientProtocol, userInputIndex);
            // wprintw(messageWindow, "CLIENT SIDE DEBUG: Sent: %s\n", sendBuffer);
            // wrefresh(messageWindow);
            memset(sendBuffer, 0, sizeof(sendBuffer));
            memset(clientProtocol, 0, sizeof(clientProtocol));
            userInputIndex = 0;
            werase(userInputWindow);
            box(userInputWindow, 0, 0);
            mvwprintw(userInputWindow, 1, 1, "> ");
            wmove(userInputWindow, 1, 3);
            wrefresh(userInputWindow);
        }

        // If the user enters ANYTHING other than ENTER
        else if (currentCharacterAscii != '\n')
        {
            // Add character to buffer
            if (userInputIndex < MAX_MESSAGE_SIZE - 1)
            {
                // Increment the userInputIndex and add the character to the index location in the char array
                sendBuffer[userInputIndex++] = currentCharacterAscii;
                sendBuffer[userInputIndex] = '\0'; // Put a null terminator at the end!

                // Erase the input window after the input is stored in the buffer

                // This is how the terminal display
                // werase(userInputWindow);
                box(userInputWindow, 0, 0);
                mvwprintw(userInputWindow, 1, 1, "> %s", sendBuffer);
                wmove(userInputWindow, 1, 3 + userInputIndex);
                wrefresh(userInputWindow);
            }
        }
    }
}

/**
 * Close socket, delete ncurses windows, and end ncurses mode.
 */
void cleanup()
{
    // Close the socket
    close(socketFileDescriptor);

    // delete the message and user input windows
    delwin(messageWindow);
    delwin(userInputWindow);

    // Restore the terminal to its original state (stops ncurses windows)
    endwin();
}


void check_host_name(int hostname) { //This function returns host name for local computer
    if (hostname == -1) {
       perror("gethostname");
       exit(1);
    }
 }
 void check_host_entry(struct hostent * hostentry) { //find host info from host name
    if (hostentry == NULL){
       perror("gethostbyname");
       exit(1);
    }
 }
 void IP_formatter(char *IPbuffer) { //convert IP string to dotted decimal format
    if (NULL == IPbuffer) {
       perror("inet_ntoa");
       exit(1);
    }
 }


int main(int argc, char *argv[])
{
    char host[256];
    char *clientIP;
    struct hostent *host_entry;
    int hostname;
    hostname = gethostname(host, sizeof(host)); //find the host name
    check_host_name(hostname);
    host_entry = gethostbyname(host); //find host information
    check_host_entry(host_entry);
    clientIP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); //Convert into IP string

    printf("Current Host Name: %s\n", host);
    printf("Host IP: %s\n", clientIP);


    char serverIP[15];
    char userName[6];
    strcpy(serverIP, argv[2] + 7); // + 7 to start parsing after the -server
    strcpy(userName, argv[1] + 5); // + 5 to start parsing after the -client

    // Call the function to initialize the ncurses interfaces
    initializeNcursesWindows();

    // Try connecting to the server, if less than 0, something happened
    if (connectToServer(serverIP) < 0)  //127.0.0.1
    {
        wprintw(messageWindow, "Connect failed: %s\n", strerror(errno));
        wrefresh(messageWindow);
        cleanup();
        exit(EXIT_FAILURE);
    }

    // Ready the connection message
    wprintw(messageWindow, "Connected to server.\n");

    // Print the connection message
    wrefresh(messageWindow);

    // Start the thread to receive data from the server (background thread)
    startReceivingThread();

    // Start the function to get input from the user
    handleUserInput(userName, clientIP);

    // Clean up AFTER everything has stopped!
    cleanup();
    return 0;
}
