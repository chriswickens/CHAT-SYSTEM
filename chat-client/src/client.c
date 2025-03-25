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

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char send_buf[MAX_MESSAGE_SIZE] = {0};
    char recv_buf[MAX_MESSAGE_SIZE] = {0};

    // Initialize ncurses.
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    // Set getch() non-blocking on our input window later.
    
    int height, width;
    getmaxyx(stdscr, height, width);

    // Create two windows:
    // 1. msg_win for messages (upper portion)
    // 2. input_win for user input (bottom 3 lines)
    int msg_win_height = height - 3;
    int input_win_height = 3;
    WINDOW *msg_win = newwin(msg_win_height, width, 0, 0);
    WINDOW *input_win = newwin(input_win_height, width, msg_win_height, 0);
    
    scrollok(msg_win, TRUE);
    box(input_win, 0, 0);
    wrefresh(msg_win);
    wrefresh(input_win);

    // Create a socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        wprintw(msg_win, "Error creating socket\n");
        wrefresh(msg_win);
        endwin();
        exit(EXIT_FAILURE);
    }

    // Set the socket to non-blocking.
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        wprintw(msg_win, "fcntl F_GETFL error\n");
        wrefresh(msg_win);
        endwin();
        exit(EXIT_FAILURE);
    }
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Prepare server address.
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        wprintw(msg_win, "Invalid address/ Address not supported\n");
        wrefresh(msg_win);
        endwin();
        exit(EXIT_FAILURE);
    }

    // Connect (non-blocking connect may return EINPROGRESS).
    int res = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (res < 0 && errno != EINPROGRESS) {
        wprintw(msg_win, "Connect failed\n");
        wrefresh(msg_win);
        endwin();
        exit(EXIT_FAILURE);
    }

    // Set up pollfd for the socket.
    struct pollfd pfds[1];
    pfds[0].fd = sockfd;
    pfds[0].events = POLLIN; // watch for incoming data

    // Display initial messages.
    wprintw(msg_win, "Connected to server.\n");
    wprintw(msg_win, "Chat messages will appear here.\n");
    wrefresh(msg_win);

    // Set input_win to non-blocking.
    nodelay(input_win, TRUE);
    
    // Print prompt in input_win.
    mvwprintw(input_win, 1, 1, "> ");
    wmove(input_win, 1, 3);
    wrefresh(input_win);

    int input_index = 0;
    int ch;
    while (1) {
        // Poll for incoming messages on the socket.
        int poll_res = poll(pfds, 1, 100); // 100ms timeout
        if (poll_res > 0) {
            if (pfds[0].revents & POLLIN) {
                int n = read(sockfd, recv_buf, MAX_MESSAGE_SIZE - 1);
                if (n > 0) {
                    recv_buf[n] = '\0';
                    // Display received message in the message window.
                    wprintw(msg_win, "Received: %s\n", recv_buf);
                    wrefresh(msg_win);
                }
            }
        }

        // Check for user input from the input window.
        ch = wgetch(input_win);
        if (ch != ERR) {
            if (ch == '\n') {  // When Enter is pressed.
                if (input_index > 0) {
                    // Send the message to the server.
                    write(sockfd, send_buf, input_index);
                    wprintw(msg_win, "Sent: %s\n", send_buf);
                    wrefresh(msg_win);
                    // Clear the send buffer and the input area.
                    memset(send_buf, 0, MAX_MESSAGE_SIZE);
                    input_index = 0;
                    werase(input_win);
                    box(input_win, 0, 0);
                    mvwprintw(input_win, 1, 1, "> ");
                    wmove(input_win, 1, 3);
                    wrefresh(input_win);
                }
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (input_index > 0) {
                    input_index--;
                    send_buf[input_index] = '\0';
                    // Clear and update the input area.
                    werase(input_win);
                    box(input_win, 0, 0);
                    mvwprintw(input_win, 1, 1, "> %s", send_buf);
                    wmove(input_win, 1, 3 + input_index);
                    wrefresh(input_win);
                }
            } else {
                if (input_index < MAX_MESSAGE_SIZE - 1) {
                    send_buf[input_index++] = ch;
                    send_buf[input_index] = '\0';
                    // Update the input window with the new character.
                    werase(input_win);
                    box(input_win, 0, 0);
                    mvwprintw(input_win, 1, 1, "> %s", send_buf);
                    wmove(input_win, 1, 3 + input_index);
                    wrefresh(input_win);
                }
            }
        }
        usleep(10000); // Brief sleep to reduce CPU usage.
    }

    // Cleanup (this code is unreachable in the loop above, but included for completeness).
    close(sockfd);
    delwin(msg_win);
    delwin(input_win);
    endwin();
    return 0;
}
