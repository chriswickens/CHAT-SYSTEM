/*
FILE : chat-server.h
PROJECT : SENG2030 - Assignment #4
PROGRAMMER : Volfer Carvalho Freire, Jack Prudnikowicz, Kyle Murawsky, Chris Wickens, Melissa Reyes
FIRST VERSION : 2025-03-25
DESCRIPTION :
Contains common functions and variables used by various programs in this system
*
*/

#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include "../../Common/inc/common.h"

// Function prototypes.
int initializeListener();
void acceptConnection(int listeningSocket);
void broadcastChatMessage(char *messageToBroadcast, int senderSocket);
void processClientMessage(int clientSocket);
void *clientHandler(void *clientSocketPointer);

// defines
#define MAX_CLIENTS 10

#endif // CHAT_SERVER_H