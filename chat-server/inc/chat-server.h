#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

// #include <fcntl.h>
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