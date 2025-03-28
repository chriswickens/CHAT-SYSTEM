#ifndef COMMON_H
#define COMMON_H

// Common defines for server and client
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
// #include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/inet.h>

// Your code here
#define SERVER_PORT 8888

#define MAX_PROTOL_MESSAGE_SIZE 128

// Titles
#define CHAT_TITLE "============= Received Messages ============="

#endif