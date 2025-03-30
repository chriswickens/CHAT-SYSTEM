#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H


#include <ncurses.h>
// #include <netdb.h>

#include "../../Common/inc/common.h"
void initializeNcursesWindows(void);
int connectToServer(const char *serverIpAddress);
void *handleReceivedMessage(void *arg);
int startReceivingThread(void);
void handleUserInput(char *userName, char *clientIP);
void cleanup(void);
void getLocalIP(char *ipBuffer, size_t bufferSize);
void splitMessage(const char *fullString, char *firstPart, char *secondPart);
void checkHostName(int hostname);
void checkHostEntryDetails(struct hostent *hostentry);
void ipAddressFormatter(char *IPbuffer);
void sendProtocolMessage(const char *message);
void updateUserInputWindow(WINDOW *inputWin, const char *currentBuffer, int userInputIndex);

// Code here
#define CLIENT_INPUT_MARKER ">"



#define CLIENT_MAX_MSG_SIZE 81 // Message size used for MAX in client
#define CLIENT_MSG_PART_LENGTH 40

#endif // CHAT_CLIENT_H