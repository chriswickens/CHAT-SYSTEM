#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H


#include <ncurses.h>
#include "../../Common/inc/common.h"

// Function prototypes
void initializeNcursesWindows(void);
int connectToServer(const char *serverIpAddress, int *socketFileDescriptor);
void *handleReceivedMessage(void *arg);
int startReceivingThread(int *socketFileDescriptor);
void handleUserInput(char *clientName, char *clientIP, int *socketFileDescriptor);
void cleanup(int *socketFileDescriptor);
void getLocalIP(char *ipBuffer, size_t bufferSize);
void splitMessage(const char *fullString, char *firstPart, char *secondPart);
void checkHostName(int hostname);
void checkHostEntryDetails(struct hostent *hostentry);
void ipAddressFormatter(char *IPbuffer);
void sendProtocolMessage(const char *message, int socketFileDescriptor);
void updateUserInputWindow(WINDOW *inputWin, const char *currentBuffer, int userInputIndex);
int getUserName(char *userArg, char* userName);
int getServerAddress(char *serverArgument, char *serverAddress);
#define CLIENT_INPUT_MARKER ">"
#define CLIENT_MAX_MSG_SIZE 81 // Message size used for MAX in client
#define CLIENT_MSG_PART_LENGTH 40 // Max length of msg parts
#define CHAT_TITLE "========= RECEIVED MESSAGES ========="
#define INPUT_TITLE "========= USER INPUT ========="

#endif // CHAT_CLIENT_H