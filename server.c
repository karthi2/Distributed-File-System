#include "params.h"
#include<stdio.h>
#include<stdlib.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

#include "mysocket.h"
#include <dirent.h>
#include "mysocket.h"

#define THREAD_MAXSIZE 1000
#define TRUE 1

extern int mylog;

THREADLIST threadsList = NULL;
int serverThreadIndex = 0;
int threadLimit = 1;

char* username = NULL;
char *rootdir = NULL;
char *remoterootdir = NULL;

volatile int modify = 0;
volatile int checkpoint = 0;

pthread_mutex_t modify_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t modify_condition = PTHREAD_COND_INITIALIZER;

pthread_mutex_t checkpoint_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t checkpoint_condition = PTHREAD_COND_INITIALIZER;

typedef struct ServerInfoStore *ServerConnectInfo;
struct ServerInfoStore {
	char *hostname;
	int port;
};

void setupSyncCmd(char *hostname) {

    char *path = rootdir;
    int length = strlen(path);

	if(mylog) printf("strlen: %d\n", length);
    
	char *p = path + length -1;
    int i =0;
    while(*p != '/') {
        ++i;
        p--;
    }
    int index = length - i - 1;

    char *dest = (char *)malloc( index+1 );
    strncpy(dest, path, index );
    dest[index] = '\0';

	if(mylog) printf("%s\n", dest);

	char *remoteLoc = (char *) malloc (strlen(username) + strlen(hostname) + strlen(dest) + 3);
	strcpy(remoteLoc, username);
	strcat(remoteLoc, "@");
	strcat(remoteLoc, hostname);
	strcat(remoteLoc, ":");
	strcat(remoteLoc, dest);

	char *syncCmd[] = {"rsync", "--delete", "-zvr", rootdir, remoteLoc, NULL};
	i = 0;
	/*while(syncCmd[i]!=NULL) {
		printf("Executing rsync with parameters\n syncCmd[%d] = %s\n", i, syncCmd[i]);
		i++;
	}*/

	int rsyncFD = fork();
	int status = 0;
	if(rsyncFD == -1) {
		perror("forking rync failed\n");
		return;
	}
	if(rsyncFD == 0) {	//child process
		int ret_err = execvp(syncCmd[0], syncCmd);
		if(ret_err == -1) {
			perror("execvp failed\n");
		}
	} else {	//parent process
		int ret_err = wait(&status);
		if(ret_err == -1) {
			perror("Error waiting on child\n");
			printf("child status: %d\n", status);
		}
	}
}

int removeThreadList(pthread_t thread) {
	THREADLIST temp = threadsList, prev = NULL;
	while(temp != NULL) {
		prev = temp;
		if(pthread_equal(thread, temp->thread)) {
			break;
		}
		temp = temp->next;
	}

	if(temp == NULL) return;

	if(prev == NULL) {
		threadsList = threadsList->next;
		free(temp);
		return;
	}
	prev->next = temp->next;
	free(temp);
}	

void* serverThread(void* sock) {
	//read the msg and execute the cmd
	if(mylog) printf("Thread started\n");
	int clientSocket = (int)sock;
	//char *type = NULL;
	int ret_val;
	while(TRUE) {
		char* type = readString(clientSocket);
		if(mylog) printf("msg type: %s\n", type);
		if(NULL == type) {
			//socket was closed, close the thread
			removeThreadList(pthread_self());
			pthread_exit(NULL);
		}
		MSGTYPE msgtype = getType(type);
		switch(msgtype) {
			case OPEN:
				fileopen(clientSocket);
				break;
			case MKDIR:
				/*modify = 1;
				if(checkpoint) {
					pthread_mutex_lock( &checkpoint_mutex );
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
					pthread_mutex_unlock( &checkpoint_mutex );
				}*/

				pthread_mutex_lock( &checkpoint_mutex );
				while(checkpoint) {
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
				}
				modify++;
				pthread_mutex_unlock( &checkpoint_mutex );

				makeDir(clientSocket);

				//contend for lock after writing value to client
				pthread_mutex_lock(&checkpoint_mutex );
				modify--;
				if(modify == 0) {
					pthread_cond_broadcast(&modify_condition);
				}
				pthread_mutex_unlock(&checkpoint_mutex);
				break;
			case RMDIR:
				/*modify = 1;
				if(checkpoint) {
					pthread_mutex_lock( &checkpoint_mutex );
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
					pthread_mutex_unlock( &checkpoint_mutex );
				}*/

				pthread_mutex_lock( &checkpoint_mutex );
			    while(checkpoint) {
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
				}
				modify++;
				pthread_mutex_unlock( &checkpoint_mutex );
				
				removeDir(clientSocket);

				//contend for lock after writing value to client
				pthread_mutex_lock(&checkpoint_mutex );
				modify--;
				if(modify == 0) {
					pthread_cond_broadcast(&modify_condition);
				}
				pthread_mutex_unlock(&checkpoint_mutex);
				break;
			case READ:
				ret_val = fileread(clientSocket);
				break;
			case GETATTR:
				getAttr(clientSocket);
				break;
			case WRITE:
				/*modify = 1;
				if(checkpoint) {
					pthread_mutex_lock( &checkpoint_mutex );
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
					pthread_mutex_unlock( &checkpoint_mutex );
				}*/

				pthread_mutex_lock( &checkpoint_mutex );
			    while(checkpoint) {
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
				}
				modify++;
				pthread_mutex_unlock( &checkpoint_mutex );
				
				fileWrite(clientSocket);

				//contend for lock after writing value to client
				pthread_mutex_lock(&checkpoint_mutex );
				modify--;
				if(modify == 0) {
					pthread_cond_broadcast(&modify_condition);
				}
				pthread_mutex_unlock(&checkpoint_mutex);
				break;
			case CLOSE:
				//removeThreadList(pthread_self());
				fileclose(clientSocket);
				if(clientSocket == 0) {
					perror("client socket cannot be 0!!");
				} else {
					close(clientSocket);
				}
				//pthread_exit(NULL);
				break;
			case RELEASE:
				//removeThreadList(pthread_self());
				fileclose(clientSocket);
				if(clientSocket == 0) {
					perror("client socket cannot be 0!!");
				} else {
				//	close(clientSocket);
				}
				//pthread_exit(NULL);
				break;
			case READDIR:
				readDirectory(clientSocket);
				break;
			case OPENDIR:
				openDirectory(clientSocket);
				break;
			case CREAT:
				createFile(clientSocket);
				break;
			case MKNOD:
				pthread_mutex_lock( &checkpoint_mutex );
			    while(checkpoint) {
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
				}
				modify++;
				pthread_mutex_unlock( &checkpoint_mutex );
				
				createFileUsingMknod(clientSocket);

				//contend for lock after writing value to client
				pthread_mutex_lock(&checkpoint_mutex );
				modify--;
				if(modify == 0) {
					pthread_cond_broadcast(&modify_condition);
				}
				pthread_mutex_unlock(&checkpoint_mutex);
				break;
			case UNLINK:
				pthread_mutex_lock( &checkpoint_mutex );
			    while(checkpoint) {
					pthread_cond_wait( &checkpoint_condition, &checkpoint_mutex );
				}
				modify++;
				pthread_mutex_unlock( &checkpoint_mutex );

				deleteFile(clientSocket);
				
				//contend for lock after writing value to client
				pthread_mutex_lock(&checkpoint_mutex );
				modify--;
				if(modify == 0) {
					pthread_cond_broadcast(&modify_condition);
				}
				pthread_mutex_unlock(&checkpoint_mutex);
				break;
			case REINSTATE:
				fileopen(clientSocket);
				//reinstateFileOnServer(clientSocket);
				break;
			case SYNC:
				/*checkpoint = 1;
				if(modify) {
					//wait for the threads to complete the current modification
					pthread_mutex_lock(&count_mutex );
					pthread_cond_wait(&condition_var, &count_mutex);
					pthread_mutex_unlock(&count_mutex);
				}*/

				pthread_mutex_lock( &checkpoint_mutex );
				while(modify) {
					pthread_cond_wait( &modify_condition, &checkpoint_mutex );
				}
				checkpoint++;
				pthread_mutex_unlock( &checkpoint_mutex );

				//track the modifications in operation log
				//initiate sync
				char *secondaryHostName = readString(clientSocket);
				setupSyncCmd(secondaryHostName);

				pthread_mutex_lock(&checkpoint_mutex);
				checkpoint--;
				if(checkpoint == 0) {
					pthread_cond_broadcast(&checkpoint_condition);
				}
				pthread_mutex_unlock(&checkpoint_mutex);
				break;
		}
		free(type);
	}
	//close(clientSocket);
	//pthread_exit(NULL);
}

int addThreadList(pthread_t thread) {
	THREADLIST newThread = (THREADLIST)malloc(sizeof(struct threadlist));
	newThread->thread = thread;
	newThread->next = NULL;

	if(NULL == threadsList) {
		threadsList = newThread;
		return 0;
	}

	THREADLIST temp = threadsList;
	while(NULL != temp->next) {
		temp = temp->next;
	}
	temp->next = newThread;
}

int invokeThread(const pthread_attr_t* attr, void *(*method)(void*) , void* data) {
	pthread_t thread;
	int ret_val  = pthread_create(&thread, attr, method, data);
    if(ret_val != 0) {
		perror("Thread creation failed\n");
		return -1;
    }
	pthread_detach(thread);

	if(mylog) {
		printf("thread created!\n");
	}
	serverThreadIndex++;

	addThreadList(thread);

	if(serverThreadIndex == THREAD_MAXSIZE) {
		if(mylog) {
			printf("thread limit reached!\n");
		}
		threadLimit = TRUE;
	}
}

ServerConnectInfo readServerInfoToSync(int sock) {
	char *host = readString(sock);
	if(!strcmp(host, "NULL")) {
		if(mylog) printf("I'm the primary!\n");
		return NULL;
	}
	int port = readInt(sock);
	ServerConnectInfo info = (ServerConnectInfo)malloc(sizeof(struct ServerInfoStore));
	info->hostname = host;
	info->port = port;
	if(mylog) {
		printf("hostname: %s, port:%d\n", host, port);
	}
	return info;
}

int sendSyncMessage(int sock) {
	writeString(sock, "SYNC");
	char *myHostName = (char *)malloc( sizeof(char) * (64 + 1) );
	int ret_err = gethostname(myHostName, 65);
	if(ret_err == -1) {
		perror("Err getting host name\n");
	}
	struct hostent *he = gethostbyname(myHostName);
	char *ip = inet_ntoa(*((struct in_addr *)he->h_addr_list[0]));
	writeString(sock, ip);
}

int initiateSync(ServerConnectInfo info) {
	struct hostent *serverIP = gethostbyname(info->hostname);
	struct sockaddr_in src, dest;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		return sock;
	}

	dest.sin_family = AF_INET;
	dest.sin_port = htons(info->port);
	memcpy(&dest.sin_addr, serverIP->h_addr_list[0], serverIP->h_length);

	int ret_val = connect(sock, (struct sockaddr*)&dest, sizeof(dest));
	if(ret_val < 0) {
		perror("connection to primary failed\n");
		exit(-1);
	}

	sendSyncMessage(sock);
}

int registerServer(int serverPort, char *masterHostName,  int masterPort) {
	struct hostent *masterIP = gethostbyname(masterHostName);
	if(mylog) {
	   printf("IP address: %s\n", inet_ntoa(*((struct in_addr *)masterIP->h_addr_list[0])));
	   printf("master port: %d\n", masterPort);
	}
	struct sockaddr_in src, dest;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		return sock;
	}

	dest.sin_family = AF_INET;
	dest.sin_port = htons(masterPort);
	memcpy(&dest.sin_addr, masterIP->h_addr_list[0], masterIP->h_length);

	int ret_val = connect(sock, (struct sockaddr*)&dest, sizeof(dest));
	if(ret_val < 0) {
		perror("Connection to master failed\n");
		exit(-1);
	}

	//send the master the register message along with the listening port
	if(mylog) printf("Sending register message\n");
	writeString(sock, "REGISTER");
	writeInt(sock, serverPort);

	//read the hostname and port number of the server to sync with.
	if(mylog) printf("reading the address of the server to sync with");
	ServerConnectInfo info = readServerInfoToSync(sock);

	if(info == NULL) {
		//i'm the primary...hahahaha :)
		return 0;
	}

	//sync with the server
	if(username != NULL) {
		initiateSync(info);
	}
	//send the ready msg

}

int main(int argc, char* argv[]) {
	if(argc<4) {
		printf("./server server_port master_hostname master_port <remote-username> <rootdir>\n");
		exit(0);
	}

	int serverPort = atoi(argv[1]);
	struct sockaddr_in sin;
	int serverSocket = openServerSocket(serverPort, &sin);
	if(serverSocket < 0) {
		perror("could not start the server\n");
		exit(-1);
	}

	char *masterHostName = argv[2];
	int masterPort = atoi(argv[3]);

	if(argc == 6) {
		username = (char *) malloc( strlen(argv[4]) + 1);
		strcpy(username, argv[4]);
		rootdir = (char *) malloc( strlen(argv[5]) + 1);
		strcpy(rootdir, argv[5]);
	}

	if(mylog)
		printf("registering the server with the master\n");
	int ret_val = registerServer(serverPort, masterHostName, masterPort);
	if(ret_val < 0) {
		perror("Registering with master failed\n");
		exit(-1);
	}

	int client_socket = -1;
	struct sockaddr_in client;
	while(TRUE) {
		int len = sizeof(client);
		if(mylog) {
			printf("waiting for incoming connections\n");
		}
		client_socket = accept(serverSocket, (struct sockaddr*)&client, &len);
		if(client_socket == 0) {
			perror("client socket 0!\n");
		}
		if(client_socket == -1) {
			perror("Accept failed\n");
			exit(-1);
		}
		
		if(threadLimit) {
		}

		if(mylog) {
			printf("invoking thread...\n");
		}
		int retVal  = invokeThread(NULL, serverThread, (void*)client_socket);
		if(ret_val == -1) {
			perror("Thread invocation failed\n");
			exit(-1);
		}
	}
}
