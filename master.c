#include "params.h"
#include <stdio.h>
#include <stdlib.h>
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
#include "master.h"

extern int mylog;

void sendServerInfo_open(int sock) {
	//int ret_val = checkLoad();
	int ret_val = 0;
	if(mylog) {
		printf("sending host: %s\n", primaryServer->hostname);
		printf("sending port: %d\n", primaryServer->port);
	}
	if(ret_val == 0) {
		writeString(sock, primaryServer->hostname);
		writeInt(sock, primaryServer->port);
	} else if(ret_val == 1) {
		writeString(sock, head->next->hostname);
		writeInt(sock, head->next->port);
	}
}

void sendServerInfo(int sock) {
	//int ret_val = checkLoad();
	int ret_val = 0;
	if(mylog) {
		printf("sending host: %s\n", primaryServer->hostname);
		printf("sending port: %d\n", primaryServer->port);
	}
	if(ret_val == 0) {
		writeString(sock, primaryServer->hostname);
		writeInt(sock, primaryServer->port);
	} /*else if(ret_val == 1) {
		writeString(sock, serverHandle[1]->hostname);
		writeInt(sock, serverHandle[1]->port);
	}*/
}

void updateAvailableServersInfo(SERVER failedServer) {
	if(head == NULL) {
		if(mylog) printf("There has to be atleast one server in the list always!! This should never happen\n");
		return;
	}

	SERVER temp = head;
	SERVER prev = NULL;

	while(temp!=NULL) {
		if(!strcmp(failedServer->hostname,temp->hostname) && failedServer->port == temp->port ) {
			break;
		}
		prev = temp;
		temp = temp->next;
	}

	if(temp == NULL) return;

	if(prev == NULL) {
		head = head->next;
		free(temp);
		return;
	}
	prev->next = temp->next;
	free(temp);
}

/*void updateAvailableServersInfo(SERVER failedServer) {
	int i =0;
	for(i=0;i<5;++i) {
		if( !strcmp(failedServer->hostname,serverHandle[i]->hostname) && failedServer->port == serverHandle[i]->port ) {
			serverHandle[i] = NULL; //_REVISIT_ free
			break;
		}
	}
	strcpy(primaryServer->hostname, serverHandle[i+1]->hostname);
	primaryServer->port = serverHandle[i+1]->port;
}*/

void recvFailedServerInfo(int sock) {
	SERVER failedServer = (SERVER) malloc (sizeof(struct server));
	strcpy(failedServer->hostname, readString(sock));	//_REVISIT_ no need for strcpy
	failedServer->port = readInt(sock);

	updateAvailableServersInfo(failedServer);
	//_REVISIT_ if there is only one server and it is removed
	strcpy(primaryServer->hostname, head->hostname);
	primaryServer->port = head->port;
}

void sendPrimaryInfo(int sock) {
	writeString(sock, primaryServer->hostname);
	writeInt(sock, primaryServer->port);
}

void addToServerList(SERVER server_info) {
	if(head == NULL) {
		head = server_info;
		return;
	}
	SERVER temp = head;
	while(temp->next!=NULL) {
		temp = temp->next;
	}
	temp->next = server_info;
}

int isSync(){
	return serverHandleIndex;
}

int parseMSG(int sock) {
	char *buf = readString(sock);
	if(buf == NULL) {
		FD_CLR(sock, &read_flags);
		return -1;
	}
	if(mylog) {
		printf("msg type %s\n", buf);
	}
	char *fpath = NULL;
	int i = 0;
    switch(getType(buf)) {
        case REGISTER:  //register a server
			if(mylog)printf("Inside switch 1%d", ++i);
			registerServer(sock);
			if(isSync()) {
				sendPrimaryInfo(sock);
			}
            break;
        case CREAT:
			if(mylog)printf("Inside switch 2%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
            break;
        case OPEN:
			if(mylog)printf("Inside switch 3%d", ++i);
			fpath = readString(sock);
			sendServerInfo_open(sock);
            break;
        case CLOSE:
			if(mylog)printf("Inside switch 4%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
            break;
        case RELEASE:
			if(mylog)printf("Inside switch 4%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
            break;
		case READ:
			if(mylog)printf("Inside switch 5%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case WRITE:
			if(mylog)printf("Inside switch 6%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case MKDIR:
			if(mylog)printf("Inside switch 7%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case RMDIR:
			if(mylog)printf("Inside switch 8%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case GETATTR:
			if(mylog)printf("Inside switch 9%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case READDIR:
			if(mylog)printf("Inside switch 10%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case OPENDIR:
			if(mylog)printf("Inside switch 11%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case MKNOD:
			if(mylog)printf("Inside switch 12%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case UNLINK:
			if(mylog)printf("Inside switch 13%d", ++i);
			fpath = readString(sock);
			sendServerInfo(sock);
			break;
		case FAILEDSERVER:
			recvFailedServerInfo(sock);
			break;
		case SYNC:
			if(mylog)printf("Inside switch 14%d", ++i);
			sendServerInfo(sock);
			break;
		default:
			if(mylog)printf("Inside switch 15%d", ++i);
			break;
    }
	free(buf);
	free(fpath);
}

//Load Balancing
/*return 1 or 2 to indicate the server number*/
int checkLoad() {
	if(head->next!=NULL) {
		if((head->num_of_files >= (head->next->num_of_files)*2)) {
			head->next->num_of_files++;
			return 1;
		} 
	}
	head->num_of_files++;
	return 0;
}

void talkToClient() {
	//Accept the connection from client
	//Update the active set of files list
	//Remember to remove the file from the active list once the file is closed
	
}

void talkToServer() {

}


int registerServer(int sock) {
	struct hostent *ihp = gethostbyaddr((char *)&incoming.sin_addr, sizeof(struct in_addr), AF_INET);

	SERVER server_info = (SERVER) malloc(sizeof(struct server));
	strcpy(server_info->hostname, ihp->h_name);
	server_info->port = readInt(sock);
	server_info->num_of_files = 0;
	server_info->next =  NULL;

	//serverHandle[++serverHandleIndex] = server_info;
	++serverHandleIndex;
	addToServerList(server_info);

	if(serverHandleIndex == 0) {
		primaryServer = (SERVER) malloc (sizeof(struct server));
		strcpy(primaryServer->hostname, server_info->hostname);
		primaryServer->port = server_info->port;
		writeString(sock, "NULL");
	} else {
		/*initiate rsync*/
		/* 1. Send servers a msg that it has to being rsync with the other one
		 * 2. Wait for rsync to complete. Check for return value
		 */
		writeString(sock, primaryServer->hostname);
		writeInt(sock, primaryServer->port);
	}
	return 1;
}

int waitOnSelect() {
	/* All incoming connections from clients and servers will be served from here */
	int max = master_socket;
	int ret_val = 0;
	int i;

	int len = sizeof(incoming);
	do {
		memcpy(&temp_flags, &read_flags, sizeof(read_flags));
		ret_val = select(max+1, &temp_flags, NULL, NULL, NULL);
		if(ret_val < 0) {
			perror("Failed on select\n");
			exit(ret_val);
		}

		if(FD_ISSET(master_socket, &temp_flags)) {
			if(mylog) {
				printf("accepting a new connection\n");
			}
			int new_socket = accept(master_socket, (struct sockaddr *)&incoming, &len);
			if(new_socket < 0) {
				perror("Unable to accept\n");
				return new_socket;
			}
			parseMSG(new_socket);
			//int new_socket = registerServer();
			FD_SET(new_socket, &read_flags);
			if(new_socket > max) {
				max = new_socket;
			}
			FD_CLR(master_socket, &temp_flags);
		}

		/* check if there was any other incoming connection, either server or client */
		for(i=0;i<=max;i++) {
			if(FD_ISSET(i, &temp_flags)) {
				parseMSG(i);
			}
		}
	} while(1);
}

void setup_master(int master_port) {
    int option = 1;
    /* use address family INET and STREAMing sockets (TCP) */
    master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket < 0) {
        perror("socket:");
        exit(master_socket);
    }

    /* set up the address and port */
    master_in.sin_family = AF_INET;
    master_in.sin_port = htons(master_port);
    memcpy(&master_in.sin_addr, master->h_addr_list[0], master->h_length);

    /* bind socket s to address sin */
    int rc = bind(master_socket, (struct sockaddr *)&master_in, sizeof(master_in));
    if (rc < 0) {
            perror("bind:");
            close(master_socket);
            exit(rc);
    }

    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
    rc = listen(master_socket, 5);
    if (rc < 0) {
        perror("listen:");
        close(master_socket);
        exit(rc);
    }
}

void usage() {
	printf("Usage:\n./master <port number>\n");
	return;
}

void main(int argc, char **argv) {
	int master_port = 0;  //Port number on which master will listen

	if(argc!=2) {
		printf("Very few arguments for master\n");
		usage();
		exit(0);
	}

	master_port = atoi(argv[1]);
	if(master_port <=1024) {
		printf("Bad port number\n");
		exit(0);
	}

	/* fill in hostent struct for self */
    if((gethostname(host, sizeof(host))) == -1) {
        perror("gethostname ");
        exit(-1);
    }
    master = gethostbyname(host);
    if (master == NULL) {
        fprintf(stderr, "%s: host not found (%s)\n", (char *)argv[0], host);
        exit(-1);
    }
	if(mylog) {
		printf("master ip: %s, master port: %d\n", inet_ntoa(*((struct in_addr *)master->h_addr_list[0])), master_port);
	}

	if(mylog) printf("setting up master\n");
	setup_master(master_port);

	FD_ZERO(&read_flags);
	FD_SET(master_socket, &read_flags);
	waitOnSelect();
	
	/* All servers have to register with the master upon startup */
	//ret_val = registerServer();
}
