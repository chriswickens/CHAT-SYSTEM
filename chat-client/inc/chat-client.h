#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H


#include <ncurses.h>
// #include <netdb.h>
#include <arpa/inet.h>
#include "../../Common/inc/common.h"


// Code here
#define CLIENT_INPUT_MARKER ">"

#define CLIENT_MAX_MSG_SIZE 81 // Message size used for MAX in client
#define CLIENT_MSG_PART_LENGTH 40

#endif // CHAT_CLIENT_H