#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 8888    // Port number of chat server
#define MAX_MESSAGE_SIZE 80 // Maximum length of a chat message

int socketFileDescriptor;                // Global socket descriptor
WINDOW *messageWindow, *userInputWindow; // ncurses windows for chat display and input
char receiveBuffer[MAX_MESSAGE_SIZE];    // Buffer for incoming messages

// Function prototypes
void initializeNcursesWindows(void);
int connectToServer(const char *ip);
void *handleReceivedMessage();
int startReceivingThread(void);
void handleUserInput(void);
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
int connectToServer(const char *ip)
{
    struct sockaddr_in serverAddress;
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
        return -1;

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, ip, &serverAddress.sin_addr);

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
        ssize_t n = read(socketFileDescriptor, receiveBuffer, MAX_MESSAGE_SIZE - 1);
        if (n > 0)
        {
            receiveBuffer[n] = '\0';
            wprintw(messageWindow, "%s\n", receiveBuffer);
            wrefresh(messageWindow);
        }
        else if (n == 0)
        {
            // Server closed connection
            wprintw(messageWindow, "Server disconnected.\n");
            wrefresh(messageWindow);
            break;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // Unexpected read error
            wprintw(messageWindow, "Read error: %s\n", strerror(errno));
            wrefresh(messageWindow);
            break;
        }
        usleep(50000); // Throttle loop to reduce CPU usage
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
void handleUserInput()
{
    char sendBuffer[MAX_MESSAGE_SIZE] = {0};
    int input_index = 0;
    int ch;

    while (1)
    {
        ch = wgetch(userInputWindow);
        if (ch == ERR)
        {
            usleep(50000);
            continue;
        }
        if (ch == '\n' && input_index > 0)
        {
            // Send when Enter pressed
            write(socketFileDescriptor, sendBuffer, input_index);
            wprintw(messageWindow, "Sent: %s\n", sendBuffer);
            wrefresh(messageWindow);
            memset(sendBuffer, 0, sizeof(sendBuffer));
            input_index = 0;
            werase(userInputWindow);
            box(userInputWindow, 0, 0);
            mvwprintw(userInputWindow, 1, 1, "> ");
            wmove(userInputWindow, 1, 3);
            wrefresh(userInputWindow);
        }
        else if (ch != '\n')
        {
            // Add character to buffer
            if (input_index < MAX_MESSAGE_SIZE - 1)
            {
                sendBuffer[input_index++] = ch;
                sendBuffer[input_index] = '\0';
                werase(userInputWindow);
                box(userInputWindow, 0, 0);
                mvwprintw(userInputWindow, 1, 1, "> %s", sendBuffer);
                wmove(userInputWindow, 1, 3 + input_index);
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
    close(socketFileDescriptor);
    delwin(messageWindow);
    delwin(userInputWindow);
    endwin();
}

int main()
{
    initializeNcursesWindows();

    if (connectToServer("127.0.0.1") < 0)
    {
        wprintw(messageWindow, "Connect failed: %s\n", strerror(errno));
        wrefresh(messageWindow);
        cleanup();
        exit(EXIT_FAILURE);
    }

    wprintw(messageWindow, "Connected to server.\n");
    wrefresh(messageWindow);

    startReceivingThread();
    handleUserInput();

    cleanup();
    return 0;
}
