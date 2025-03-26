#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#define SERVER_PORT 8888
#define BUFFER_SIZE 1024
#define MAX_MESSAGE_SIZE 80

int main()
{
    int socketFileDescriptor;
    struct sockaddr_in serverAddress;
    char sendBuffer[MAX_MESSAGE_SIZE] = {0};
    char receiveBuffer[MAX_MESSAGE_SIZE] = {0};

    // Initialize ncurses.
    initscr();
    cbreak();
    noecho();
    // We dont need the keypad enabled?
    // keypad(stdscr, TRUE);

    // Set getch() non-blocking on our input window later.

    int height, width;
    getmaxyx(stdscr, height, width);

    // Create two windows:
    // 1. messageWindow for messages (upper portion)
    // 2. userInputWindow for user input (bottom 3 lines)
    int messageWindowHeight = height - 3;
    int userInputWindowHeight = 3;
    WINDOW *messageWindow = newwin(messageWindowHeight, width, 0, 0);
    WINDOW *userInputWindow = newwin(userInputWindowHeight, width, messageWindowHeight, 0);

    scrollok(messageWindow, TRUE);
    box(userInputWindow, 0, 0);
    wrefresh(messageWindow);
    wrefresh(userInputWindow);

    // Create a socket.
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
    {
        wprintw(messageWindow, "Error creating socket\n");
        wrefresh(messageWindow);
        endwin();
        exit(EXIT_FAILURE);
    }

    // Set the socket to non-blocking.
    int flags = fcntl(socketFileDescriptor, F_GETFL, 0);
    if (flags < 0)
    {
        wprintw(messageWindow, "fcntl F_GETFL error\n");
        wrefresh(messageWindow);
        endwin();
        exit(EXIT_FAILURE);
    }
    fcntl(socketFileDescriptor, F_SETFL, flags | O_NONBLOCK);

    // Prepare server address.
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0)
    {
        wprintw(messageWindow, "Invalid address/ Address not supported\n");
        wrefresh(messageWindow);
        endwin();
        exit(EXIT_FAILURE);
    }

    // Connect (non-blocking connect may return EINPROGRESS).
    int res = connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (res < 0 && errno != EINPROGRESS)
    {
        wprintw(messageWindow, "Connect failed\n");
        wrefresh(messageWindow);
        endwin();
        exit(EXIT_FAILURE);
    }

    // Set up pollfd for the socket.
    struct pollfd pfds[1];
    pfds[0].fd = socketFileDescriptor;
    pfds[0].events = POLLIN; // watch for incoming data

    // Display initial messages.
    wprintw(messageWindow, "Connected to server.\n");
    wprintw(messageWindow, "Chat messages will appear here.\n");
    wrefresh(messageWindow);

    // Set userInputWindow to non-blocking.
    nodelay(userInputWindow, TRUE);

    // Print prompt in userInputWindow.
    mvwprintw(userInputWindow, 1, 1, "> ");
    wmove(userInputWindow, 1, 3);
    wrefresh(userInputWindow);

    int input_index = 0;
    int ch;
    while (1)
    {
        // Poll for incoming messages on the socket.
        int poll_res = poll(pfds, 1, 100); // 100ms timeout
        if (poll_res > 0)
        {
            if (pfds[0].revents & POLLIN)
            {
                int n = read(socketFileDescriptor, receiveBuffer, MAX_MESSAGE_SIZE - 1);
                if (n > 0)
                {
                    receiveBuffer[n] = '\0';
                    // Display received message in the message window.
                    wprintw(messageWindow, "Received: %s\n", receiveBuffer);
                    wrefresh(messageWindow);
                }
            }
        }

        // Check for user input from the input window.
        ch = wgetch(userInputWindow);
        if (ch != ERR)
        {
            if (ch == '\n')
            { // When Enter is pressed.
                if (input_index > 0)
                {
                    // Send the message to the server.
                    write(socketFileDescriptor, sendBuffer, input_index);
                    wprintw(messageWindow, "Sent: %s\n", sendBuffer);
                    wrefresh(messageWindow);
                    // Clear the send buffer and the input area.
                    memset(sendBuffer, 0, MAX_MESSAGE_SIZE);
                    input_index = 0;
                    werase(userInputWindow);
                    box(userInputWindow, 0, 0);
                    mvwprintw(userInputWindow, 1, 1, "> ");
                    wmove(userInputWindow, 1, 3);
                    wrefresh(userInputWindow);
                }
            }

            // We dont need to worry about backspace
            // else if (ch == KEY_BACKSPACE || ch == 127)
            // {
            //     if (input_index > 0)
            //     {
            //         input_index--;
            //         sendBuffer[input_index] = '\0';
            //         // Clear and update the input area.
            //         werase(userInputWindow);
            //         box(userInputWindow, 0, 0);
            //         mvwprintw(userInputWindow, 1, 1, "> %s", sendBuffer);
            //         wmove(userInputWindow, 1, 3 + input_index);
            //         wrefresh(userInputWindow);
            //     }
            // }
            else
            {
                if (input_index < MAX_MESSAGE_SIZE - 1)
                {
                    sendBuffer[input_index++] = ch;
                    sendBuffer[input_index] = '\0';
                    // Update the input window with the new character.
                    werase(userInputWindow);
                    box(userInputWindow, 0, 0);
                    mvwprintw(userInputWindow, 1, 1, "> %s", sendBuffer);
                    wmove(userInputWindow, 1, 3 + input_index);
                    wrefresh(userInputWindow);
                }
            }
        }
        usleep(10000); // Brief sleep to reduce CPU usage.
    }

    // Cleanup (this code is unreachable in the loop above, but included for completeness).
    close(socketFileDescriptor);
    delwin(messageWindow);
    delwin(userInputWindow);
    endwin();
    return 0;
}
