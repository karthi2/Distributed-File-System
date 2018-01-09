#include "mysocket.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define TRUE 1
#define RETRY_COUNT 5

extern int SOCK_CLOSE;
extern int mylog;

int masterHandle = -1;
int serverHandle = -1;
int masterPort = -1;
char *master_hostname = NULL;
char *serverHostName = NULL;
char *rootdir = NULL;
int serverPort = -1;
struct sockaddr_in in;

typedef struct my_file *FILELIST;
struct my_file {
	char *fileName;
	struct fuse_file_info *fileInfo;
	int flags;	//fileInfo has a field called flags too. Is it same?
	mode_t mode;
	FILELIST next;
};

FILELIST fileList = NULL;

int removeFileList(char *fname) {
	FILELIST temp = fileList, prev = NULL;
	while(temp != NULL) {
		if(!strcmp(fname, temp->fileName)) {
			break;
		}
		prev = temp;
		temp = temp->next;
	}

	if(temp == NULL) return 0;

	if(prev == NULL) {
		fileList = fileList->next;
		//fileList = NULL;
		free(temp->fileName);
		//temp = NULL;
		free(temp);
		return 1;
	}
	prev->next = temp->next;
	free(temp->fileName);
	//temp = NULL;
	free(temp);
	return 1;
}

int addFileList(char *fname, struct fuse_file_info *fInfo, int flag_s, mode_t m) {
	FILELIST newnode = (FILELIST) malloc(sizeof(struct my_file));
	if(newnode == NULL) return 0;

	newnode->fileName = (char *) malloc (strlen(fname) + 1);
	if(newnode->fileName == NULL) return 0;

	strcpy(newnode->fileName, fname);	//copies the NULL too
	newnode->fileInfo = fInfo;
	newnode->flags = flag_s;
	newnode->mode = m;
	newnode->next = NULL;

	if(fileList == NULL) {
		fileList = newnode;
		return 1;
	}

	FILELIST temp = fileList;
	while(temp->next != NULL) {
		temp = temp->next;
	}
	temp->next = newnode;
	return 1;
}

int reinstateFile(int serverSock, FILELIST file) {
	if(mylog) printf("reinstating file to socket:%d\n", serverSock);
	while(TRUE) {
		writeHeader(serverSock, "REINSTATE", file->fileName, 1);
		writeInt(serverSock, file->flags);

		int ret_err = readInt(serverSock);
		//file->fileInfo->fh = readInt(serverSock);
		if(ret_err != -1) {
			file->fileInfo->fh = ret_err;
			break;
		}
		return -readInt(serverSock);	//_REVISIT_ can errno be stored in file->fileInfo?
	}
}

int reinstateFiles(int serverSock) {
	if(mylog) printf("reinstating files to socket:%d\n", serverSock);
	if(fileList == NULL) {
		if(mylog) printf("file list is NULL");
	} else {
		if(mylog) printf("file list is not NULL");
	}
	FILELIST temp = fileList;
	while(temp != NULL){
		reinstateFile(serverSock, temp);
		temp = temp->next;
	}
}

int sendFailedServer() {
	writeString(masterHandle, "FAILEDSERVER");
	writeString(masterHandle, serverHostName);
	writeInt(masterHandle, serverPort);
}

int establishConnectionToServer(char *cmd, char *fpath) {
	int retry = 0;
	while(TRUE) {
		writeString(masterHandle, cmd);
		writeString(masterHandle, fpath);
		char *host = readString(masterHandle);
		int port = readInt(masterHandle);

		if(host == NULL) {
			if(serverHostName == NULL) {
				perror("could not connect to servers: host and serverName is null\n");
				return -1;
			} else {
				return 1;
			}
		}

		int reinstate = 1;
		//if the hostname and port is same as previous connection, no need to connect again
		if( serverHostName != NULL && (!strcmp(serverHostName, host)) && port == serverPort) {
			//log_msg("connection to old handle server %s, %d\n", serverHostName, serverPort);
			reinstate = 0;
		} else {
			if(serverHostName == NULL) {
				reinstate = 0;
			}
			serverHostName = host;
			serverPort = port;

			//sleep(2);
//			int devfd = open("/dev/null", O_RDONLY);
			int prevHandle = serverHandle;
			int ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
			if(mylog) printf("I prevhandle: %d, new server handle %d\n", prevHandle, serverHandle);
			if(prevHandle == serverHandle) {
				ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
				if(close(prevHandle) == -1) {
					if(mylog) fprintf(stdout, "Error closing socket: %d\n", prevHandle);
					if(mylog) perror("Err closing socket");
				}
			}
			if(mylog) printf("II prevhandle: %d, new server handle %d\n", prevHandle, serverHandle);
//			close(devfd);
			if(ret_val == -1) {
				if(mylog) printf("connection to new serverfailed, retrying...III prevhandle: %d, new server handle %d\n", prevHandle, serverHandle);
				if(retry == RETRY_COUNT){
					return  -1;
				}
				sendFailedServer();
				retry++;
				continue;
			}
		}

		if(reinstate && SOCK_CLOSE) {
			SOCK_CLOSE = 0;
			//log_msg("Reconnecting to server %s, %d\n", serverHostName, serverPort);
			if(mylog) printf("IV new server handle %d\n", serverHandle);
			reinstateFiles(serverHandle);
		}
		break;
	}
	return 0;
}

static void my_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here
}

int checkSocketClose(int *socketClosed, int *retryCnt) {
	if(*socketClosed != 0)  {	//the current server has failed, trying a different server
		//log_msg("%s, %d failed. Trying a different server\n", serverHostName, serverPort);
		//*socketClosed = 0;
		(*retryCnt)++;
		return 1;
	}
	return 0;
}

int my_getattr(const char *path, struct stat *statbuf)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    
    my_fullpath(fpath, path);
    

	/*writeString(masterHandle, "GETATTR");
	writeString(masterHandle, fpath);
	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}*/

	int ret_err = 0;
	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}

		ret_err = establishConnectionToServer("GETATTR", fpath);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}

		int ret_err = writeHeader(serverHandle, "GETATTR", fpath, 1);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		} 
		else if(ret_val != 0) {
			ret_val = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_val;
		} 
		else {
			int err = readStatBuf(serverHandle, statbuf);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			break;
		} 
	}
   	//ret_val = lstat(fpath, statbuf); 
    //log_stat(statbuf);
	//close(serverHandle);
	//free(serverHostName);
    
    return ret_val;
}

int my_mknod(const char *path, mode_t mode, dev_t dev)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    
    my_fullpath(fpath, path);
    

	ret_val = my_open_mknod(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);	//not using the second argument
	//close(serverHandle);
	//free(serverHostName);
    
    return ret_val;
}

/** Create a directory */
int my_mkdir(const char *path, mode_t mode)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    
    my_fullpath(fpath, path);
    

	/*writeString(masterHandle, "MKDIR");
	writeString(masterHandle, fpath);
	Master should return the handle of the server

	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}*/

	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}

		int ret_err = establishConnectionToServer("MKDIR", fpath);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}

		writeHeader(serverHandle, "MKDIR", fpath, 1);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		writeInt(serverHandle, mode);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		if(ret_val < 0) {
			//read the errno from the server
			ret_val = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_val;
		} else break;
	}
	//close(serverHandle);
	//free(serverHostName);
    
    return ret_val;
}

int my_unlink(const char *path)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    my_fullpath(fpath, path);

	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}

		int ret_err = establishConnectionToServer("UNLINK", fpath);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}
		
		writeHeader(serverHandle, "UNLINK", fpath, 1);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		if(ret_val < 0) {
			int err = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -err;
		} else {
			removeFileList(fpath);
			break;
		}
		//reconnect
	}
    
    return ret_val;
}

int my_rmdir(const char *path)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    
    my_fullpath(fpath, path);
    
	/*writeString(masterHandle, "RMDIR");
	writeString(masterHandle, fpath);
	Master should return the handle of the server

	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}*/

	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}

		int ret_err = establishConnectionToServer("RMDIR", fpath);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}

		writeHeader(serverHandle, "RMDIR", fpath, 1);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		if(ret_val < 0) {
			//read the errno from the server
			ret_val = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_val;
		} else break;

		if(ret_val != -1) break;
	}
	//close(serverHandle);
	//free(serverHostName);
    
    return ret_val;
}

int reconnect() {
	//get the server handle again from master
	//serverHandle = ;
	/*if(connect_server(&host, port, *masterHandle, &in) == -1) {
		perror("Failed to connect to server\n");
		exit(-1);
	}*/

}

int my_open(const char *path, struct fuse_file_info *fi)
{
    int ret_val = 0;
    int fd;
    char fpath[PATH_MAX];
    
    my_fullpath(fpath, path);
    
	/* code for halting the client when rsync is underway */
	//while(isBusy()) {}

	//log_msg("\nOPEN --> WRITING TO THE MASTER\n");


	/*writeString(masterHandle, "OPEN");
	writeString(masterHandle, fpath);
	 Master should return the handle of the server

	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}*/

	//MESSAGE msg = createMessage("OPEN", fpath);
	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}
		
		int ret_err = establishConnectionToServer("OPEN", fpath);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}

		writeHeader(serverHandle, "OPEN", fpath, 1);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		writeInt(serverHandle, fi->flags);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		fd = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		if(fd != -1) {
			break;
		}
		else  if(fd < 0) {
			int err = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -err;
		}
	}
	
    fi->fh = fd;
	addFileList(fpath, fi, fi->flags, NULL);
    
	/* close the connection to the server */
	//close_conn();
	//free(serverHostName);
	//close(serverHandle);

    return 0;
}

int my_open_mknod(const char *fpath, int flags, int mode)
{
    int fd = 0;
    
	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}
		
		int ret_err = establishConnectionToServer("MKNOD", fpath);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}

		writeHeader(serverHandle, "MKNOD", fpath, 1);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeInt(serverHandle, flags);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		writeInt(serverHandle, mode);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		fd = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		if(fd != -1) {
			break;
		} else if(fd < 0) {
			int ret_err = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_err;
		}
	}
	
    //fi->fh = fd;
	/* close the connection to the server */
	//close_conn();
	//close(serverHandle);

    return 0;  //fd is returned back to the user
}

int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int ret_val = 0;
    
	/*writeString(masterHandle, "READ");
	writeString(masterHandle, (char *)path);
	Master should return the handle of the server

	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}*/
    
	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}
		
		int ret_err = establishConnectionToServer("READ", path);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}

		writeHeader(serverHandle, "READ", (char*)path, 0);	//_REVISIT_ path needed?
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeInt(serverHandle, fi->fh);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeInt(serverHandle, size);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		writeInt(serverHandle, offset);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		if(ret_val < 0) {
			//read the errno from the server
			ret_val = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_val;
		} else {
			/*char *my_buf = readString(serverHandle);
			int bufindex = 0;
			for(bufindex = 0; bufindex < strlen(my_buf) ;bufindex++) {
				if(my_buf[bufindex] ==  '\0') break;
				buf[bufindex] = my_buf[bufindex];
			}
			//strcpy(buf, my_buf);
			free(my_buf);//_REVISIT_*/
			readStringCustom(serverHandle, &buf);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			break;
		}
	}
	//close(serverHandle);
	//free(serverHostName);
    
    return ret_val;
}

int my_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int ret_val = 0;
    
	/*writeString(masterHandle, "WRITE");
	writeString(masterHandle, (char *)path);
    Master should return the handle of the server

	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}*/
	
	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}
		
		int ret_err = establishConnectionToServer("WRITE", path);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}
		
		writeHeader(serverHandle, "WRITE", (char*)path, 0);	//_REVISIT_ path needed?
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeInt(serverHandle, fi->fh);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeString(serverHandle, (char*)buf);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeInt(serverHandle, size);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		writeInt(serverHandle, offset);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}

		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		if(ret_val < 0) {
			//read the errno from server
			ret_val = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_val;
		} else break;
	}
    
	//close(serverHandle);
	//free(serverHostName);
    return ret_val;
}



int my_release(const char *path, struct fuse_file_info *fi)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    
    my_fullpath(fpath, path);

	int retry = 0;
	while(TRUE) {
		if(retry > 0) {
			sendFailedServer();
		}
		
		int ret_err = establishConnectionToServer("RELEASE", path);
		if(ret_err == -1) {	//failed to connect to servers
			//log_msg("Failed to establish connection to servers\n");
			break;
		}
		
		writeHeader(serverHandle, "RELEASE", (char*)path, 0);	//_REVISIT_ path needed?
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		writeInt(serverHandle, fi->fh);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		
		ret_val = readInt(serverHandle);
		if(checkSocketClose(&SOCK_CLOSE, &retry)) {
			continue;
		}
		if(ret_val < 0) {
			//read the errno from server
			ret_val = readInt(serverHandle);
			if(checkSocketClose(&SOCK_CLOSE, &retry)) {
				continue;
			}
			return -ret_val;
		} else break;
	}

    //ret_val = close(fi->fh);
	removeFileList(fpath);
    
    return ret_val;
}


int my_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int ret_val = 0;
    char fpath[PATH_MAX];
    int fd;
    
    my_fullpath(fpath, path);
    
	writeString(masterHandle, "CREAT");
	writeString(masterHandle, fpath);
	/* Master should return the handle of the server */

	serverHostName = readString(masterHandle);
	serverPort = readInt(masterHandle);
	ret_val = connect_server(serverHostName, serverPort, &serverHandle, &in);
	
	if(ret_val == -1) {
		if(!reconnect()) {
			abort();
		}
	}

	//MESSAGE msg = createMessage("OPEN", fpath);
	while(TRUE) {
		writeHeader(serverHandle, "CREAT", fpath, 1);
		writeInt(serverHandle, 0777);

		fd = readInt(serverHandle);
		if(fd < 0) {
			ret_val = readInt(serverHandle);
			return -ret_val;
		} else break;
		if(!reconnect()) {
			abort();
		}
	}
    
    fi->fh = fd;
    
	//close(serverHandle);
	//free(serverHostName);
    
    return 0;
}


struct fuse_operations my_oper = {
  .getattr = my_getattr,
  .mknod = my_mknod,
  .mkdir = my_mkdir,
  .unlink = my_unlink,
  .rmdir = my_rmdir,
  .open = my_open,
  .read = my_read,
  .write = my_write,
  .release = my_release,
};

int main(int argc, char *argv[])
{
    int i;
    int fuse_stat;

	signal(SIGPIPE, SIG_IGN);

    if (argc != 5) {
		fprintf(stderr, "usage:  client mountPoint rootdir master_host master_port\n");
		abort();
	}

	rootdir = (char *)argv[2];
    master_hostname = (char*)argv[3];
    masterPort = atoi((char*)argv[4]);

	/*argv[3] = argv[1];
	argv[1] = "-s";
	argv[2] = "-d";
	argv[4] = NULL;*/
	argv[2] = argv[1];
	argv[1] = "-s";

    if(connect_server(master_hostname, masterPort, &masterHandle, &in) == -1) {
	    perror("Cannot connect to the master\n");
        exit(-1);
    }
	writeString(masterHandle, "kolaveri");

    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(3, argv, &my_oper, NULL);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
