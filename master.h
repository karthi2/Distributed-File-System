#include<stdio.h>
#include<stdlib.h>
#include <sys/select.h>
#include "params.h"

struct hostent *master;
struct sockaddr_in master_in;
struct sockaddr_in incoming;
int mport;
int master_socket = 0;

char host[64];

typedef struct server *SERVER;
struct server {
	char hostname[64];
	int num_of_files;
	int port;
	SERVER next;
};

typedef struct client *CLIENT;
struct client {
	LIST files;	
};

typedef struct myfile *MYFILE;
struct myfile {
//	ulong fileID;
	char* filename;
	LIST clients;
	LIST servers;
};

fd_set read_flags, temp_flags;
SERVER primaryServer;

//SERVER serverHandle[5] = {NULL, NULL, NULL, NULL, NULL};
SERVER head = NULL;
int serverHandleIndex = -1;
