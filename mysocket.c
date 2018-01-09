#include "params.h"

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

#include <dirent.h>
#include "mysocket.h"

int mylog = 0;
int SOCK_CLOSE = 0;
int my_log(char *str) {
	if(mylog) perror(str);
}

int checkErrNoHostName() {
	switch(errno) {
		case EINVAL:
                        my_log("Invalid len\n");
                        break;
		case EFAULT:
                        my_log("name is an invalid address\n");
                        break;
		case ENAMETOOLONG:
                        my_log("len is smaller than the actual size\n");
                        break;
		case EPERM:
			my_log("caller did not have CAP_SYS_ADMIN capability");
			break;
		default:
			my_log("default case\n");
	}
}

int checkErrNo() {
	//clearbuffer(stderr);
	//printf("Errno in method:%d\n",errno);
	switch(errno) {
		case EACCES:
			my_log("Permission Denied\n");
			break;
		case EFAULT:
			my_log("pathname outside accessible address space\n");
			break;
		case ENOENT:
			my_log("The file does not exist\n");
			break;
		case ERANGE:
			my_log("Size of argument is less than the length of the current directory name\n");
			break;
		case ENOTDIR:
			my_log("A component of path is not a dir\n");
			break;
		case ENOMEM:
			my_log("Insufficient memory\n");
			break;
		case EINVAL:
			my_log("Invalid name\n");
			break;
		case EPERM:
			my_log("No permisson\n");
			break;
		case ENOEXEC:
			my_log("cannot execute this\n");
			break;
		case EISDIR:
			my_log("Permission Denied: Its a dir\n");
			break;
		case ELIBBAD:
			my_log("Not an executable\n");
			break;
		case EIO:
			my_log("An I/O error occurred\n");
			break;
		case ELOOP:
			my_log("Too many symbolic links were encountered in resolving path\n");
			break;
		case ENAMETOOLONG:
			my_log("path is too long\n");
			break;
		case EBADF:
			my_log("Not a valid file descriptor\n");
			break;
		case EWOULDBLOCK:
			my_log("Specified as nono-blocking socket, but would block\n");
			break;
		case ECONNRESET:
			my_log("connection reset by peer\n");
			SOCK_CLOSE = 1;
			break;
		case EPIPE:
			my_log("broken pipe err\n");
			SOCK_CLOSE = 1;
			break;
		case EMSGSIZE:
			my_log("The socket type requires that message be sent atomically, and the size of the message to be sent made this impossible\n");
			break;
		case EOPNOTSUPP:
			my_log("Some bit in the flags argument is inappropriate for the socket type\n");
			break;
		default:
			my_log("default case\n");
	}
//	errno = 0;
}

int integerToString(unsigned int num,unsigned char *p) {
        unsigned int tmp = num;
        p[0] = num >> 24;
        if(mylog) printf("0: %d\n", p[0]);
        tmp = num;
        p[1] = num >> 16;
        if(mylog) printf("1: %d\n", p[1]);
        tmp = num;
        p[2] = num >> 8;
        if(mylog) printf("2: %d\n", p[2]);
        p[3] = num;
        if(mylog) printf("3: %d\n", p[3]);
}

int stringToInteger(unsigned char *p) {
        int num = 0;
        int temp = 0;

        temp = (int)p[0];
        temp = temp << 24;
        num = num | temp;

        temp = (int)p[1];
        temp = temp << 16;
        num = num | temp;

        temp = (int)p[2];
        temp = temp << 8;
        num = num | temp;

        num = num | p[3];

        return num;
}

int writeInt(int sock, int num) {
//    sprintf(idbuf, "%d", playerID);
    //int tmp = htonl(num);
	if(mylog) printf("writing int to socket:%d\n", sock);
    int tmp = num;

    int len = -1;
    
    unsigned char *intP = (char*)&tmp;
    
    len = send(sock, intP, sizeof(tmp), MSG_NOSIGNAL);

    if(len == -1) {
		checkErrNo();
		return len;
	}

    if(mylog) printf("wrote %d, sent %d bytes\n", num, len);

}

int openServerSocket(int port, struct sockaddr_in* sin) {
  char host[64];
  int s, rc;
  struct hostent *bhp;

  /* fill in hostent struct for self */
  int retVal = gethostname(host, sizeof host);
  if(retVal == -1) {
	checkErrNoHostName();
	exit(retVal);
  }

  bhp = gethostbyname(host);
  if ( bhp == NULL ) {
    fprintf(stderr, "%s: host not found \n", host);
    exit(1);
  }
  if(mylog) {
        printf("IP address: %s\n", inet_ntoa(*((struct in_addr *)bhp->h_addr_list[0])));
  }

  /* open a socket for listening
   * 4 steps:
   *	1. create socket
   *	2. bind it to an address/port
   *	3. listen
   *	4. accept a connection
   */

  /* use address family INET and STREAMing sockets (TCP) */
  s = socket(AF_INET, SOCK_STREAM, 0);
  if ( s < 0 ) {
    perror("socket:");
    exit(s);
  }

  /* set up the address and port */
  sin->sin_family = AF_INET;
  sin->sin_port = htons(port);
  memcpy(&sin->sin_addr, bhp->h_addr_list[0], bhp->h_length);
  
  /* bind socket s to address sin */
  rc = bind(s, (struct sockaddr *)sin, sizeof(*sin));
  if ( rc < 0 ) {
    //perror("bind:");
    return -1;//exit(rc);
  }

  int optVal = 1;//any non-zero value
  int ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal));
  if(ret == -1) {
	//printf("setsockopt failed\n");
	//exit(-1);
  }

  rc = listen(s, 5);
  if ( rc < 0 ) {
    perror("listen:");
    exit(rc);
  }

  return s;

}

int printBuffer(unsigned char *p) {
	int i = 0;
	if(mylog) printf("string length: %d\n", strlen(p));
	for(i=0;i<4;++i) {
		if(mylog) printf("%d\n", p[i]);
	}
}

int readByteByByte(int s, unsigned char *p) {
	int dread = 0;
	dread = read(s, &p[3], 1);
        if(mylog) printf("3: %d\n", p[3]);
	if(dread == -1) {
		printf("Err at reading 3\n");
	}
	dread = read(s, &p[2], 1);
        if(mylog) printf("2: %d\n", p[2]);
	if(dread == -1) {
		printf("Err at reading 2\n");
	}
	dread = read(s, &p[1], 1);
        if(mylog) printf("1: %d\n", p[1]);
	if(dread == -1) {
		printf("Err at reading 1\n");
	}
	dread = read(s, &p[0], 1);
        if(mylog) printf("0: %d\n", p[0]);
	if(dread == -1) {
		printf("Err at reading 0\n");
	}
	p[4] = '\0';
        if(mylog) printf("4: %d\n", p[4]);
	printBuffer(p);
}

/*int readInt(int s) {
  int dataread = -1;
  unsigned char p[sizeof(int)+5];

  //dataread = read(s, p, sizeof(int));
  dataread = recv(s, p, sizeof(int), 0);
  if(dataread < 0) {
	perror("Error reading data\n");
  }
  return stringToInteger(p);
}*/

int readInt(int s) {
  int dataread = 0;
  int num;
	if(mylog) printf("reading int from socket:%d\n", s);
  unsigned char *p = (char*)&num;

  //dataread = read(s, p, sizeof(int));
  while(dataread != sizeof(int)) {
	  dataread  = dataread + recv(s, &p[dataread], sizeof(int)-dataread, 0);
		if(dataread <= 0) {
			//socket was closed!
			checkErrNo();
			SOCK_CLOSE = 1;
			return 0 ;
		}
	  if(mylog && dataread != sizeof(int)) printf("Expected %d, read %d\n", sizeof(int), dataread);
	  if(dataread < 0) {
        	perror("Error reading data\n");
	  }
  }
  //int val = ntohl(num);
  //int val = stringToInteger(p);
  if(mylog) printf("Read Integer:%d\n", num);
  return num;
}

int writeString(int s, char *p) {
	if(mylog) printf("writing string to socket:%d\n", s);
	//write the length of the string, followed by the actual string
	int err = writeInt(s, strlen(p));
	if(err == -1) return err;
	int len = send(s, p, strlen(p), MSG_NOSIGNAL);
	if(len == -1) checkErrNo();
	return len;
}

void readStringCustom(int s, char **p) {
	int size = readInt(s);//read the size of the string
	int dataread = 0;
	while(dataread != size) {
		dataread = dataread + recv(s, p[dataread], size-dataread, 0);
		if(dataread <= 0) {
			//socket was closed!
			checkErrNo();
			SOCK_CLOSE = 1;
			return ;
		}
   		if(mylog && dataread != size) printf("Expected %d, read %d\n", size, dataread);
                if(dataread < 0) {
			perror("Error reading data\n");
		}
	}
	//p[size] = '\0';
	return;
}

char* readString(int s) {
	int size = readInt(s);//read the size of the string
	if(size <= 0)return NULL;
	char *p = (char *)malloc (size+1);
	int dataread = 0;
	while(dataread != size) {
		dataread = dataread + recv(s, &p[dataread], size-dataread, 0);
		if(dataread <= 0) {
			//socket was closed!
			checkErrNo();
			SOCK_CLOSE = 1;
			return NULL;
		}
   		if(mylog && dataread != size) printf("Expected %d, read %d\n", size, dataread);
                if(dataread < 0) {
			perror("Error reading data\n");
		}
	}
	p[size] = '\0';
	return p;
}

int writeMSG(int s, char *intbuf, int hops, int nInts, int *arr, int id) {
        writeString(s, intbuf);
        writeInt (s, hops);
        writeInt (s, nInts);
        int i = 0;
        for(i=0;i<nInts-1;++i) {
                writeInt(s, arr[i]);
        }
        writeInt(s, id);
}

int readMSG(int s, char *type, int *hops, int *nInts, int **arr) {
      type = readString(s);
      if(!strcmp(type, "END")) {
        return 1;
      }
      *hops = readInt (s);
      *nInts = readInt(s);
      if(*nInts > 0) {
	int *a = (int *) malloc( (*nInts) * sizeof(int));
	int i = 0;
	for(i=0;i<*nInts;++i) {
	 a[i] = readInt(s);
      	}
	*arr = a;
      }
      return 0;
}

int connect_server(char *host, int port, int *sock, struct sockaddr_in* in) {
	struct hostent* dest;
	dest = gethostbyname(host);
	if ( dest == NULL ) {
		perror("gethostbyname FAIL!\n");
	}
	
	*sock = socket(AF_INET, SOCK_STREAM, 0);
	in->sin_family = AF_INET;
	in->sin_port = htons(port);
	memcpy(&in->sin_addr, dest->h_addr_list[0], dest->h_length);

	int ret_val = connect(*sock, (struct sockaddr*)in, sizeof(struct sockaddr_in));
	return ret_val;
}

MESSAGE createMessage(char *operation, char *fname) {
	MESSAGE msg = (MESSAGE) malloc (sizeof(struct Message));
	msg->operation = operation;
	msg->fname = fname;
	return msg;
}

MSGTYPE getType(char *type) {
	if(!strcmp(type, "REGISTER")) {
		return REGISTER;
	} else if(!strcmp(type, "OPEN")) {
		return OPEN;
	} else if(!strcmp(type, "MKDIR")) {
		return MKDIR;
	} else if(!strcmp(type, "RMDIR")) {
		return RMDIR;
	} else if(!strcmp(type, "READ")) {
		return READ;
	} else if(!strcmp(type, "WRITE")) {
		return WRITE;
	} else if(!strcmp(type, "GETATTR")) {
		return GETATTR;
	} else if(!strcmp(type, "READDIR")) {
		return READDIR;
	} else if(!strcmp(type, "OPENDIR")) {
		return OPENDIR;
	} else if(!strcmp(type, "SYNC")) {
		return SYNC;
	} else if(!strcmp(type, "CREAT")) {
		return CREAT;
	} else if(!strcmp(type, "CLOSE")) {
		return CLOSE;
	} else if(!strcmp(type, "MKNOD")) {
		return MKNOD;
	} else if(!strcmp(type, "UNLINK")) {
		return UNLINK;
	} else if(!strcmp(type, "REINSTATE")) {
		return REINSTATE;
	} else if(!strcmp(type, "FAILEDSERVER")) {
		return FAILEDSERVER;
	} else if(!strcmp(type, "RELEASE")) {
		return RELEASE;
	} else return NOTHING;
}

int reinstateFileOnServer(int sock) {

}

int fileopen(int sock) {
	char *fname = readString(sock);
	int flags = readInt(sock);
	int fileFD = open(fname, flags);
	if(mylog) printf("File %s opened, FD: %d\n", fname, fileFD);

	writeInt(sock, fileFD);
	if(fileFD < 0) {
		writeInt(sock, errno);
	}
	free(fname);
	return fileFD;
}

int fileclose(int sock) {
	int ret_val = close(readInt(sock));
	writeInt(sock, ret_val);
	if(ret_val < 0) {
		writeInt(sock, errno);
	}
}

int fileread(int sock) {
	int fileFD = readInt(sock);	//read file descriptor
	size_t count = (size_t)readInt(sock);	//read the number of bytes to be read
	off_t offset = (off_t)readInt(sock);	//read the offset in the file
	char *readbuf = (char*) malloc(count+1);

	int ret_val = pread(fileFD, readbuf, count, offset);
	writeInt(sock, ret_val);
	if(ret_val < 0) {
		writeInt(sock, errno);
	} else {
		readbuf[ret_val] = '\0';
	   writeString(sock, readbuf);
	}
	free(readbuf);
	return ret_val;
}

char* readHeader(int sock) {

}

int removeDir(int sock) {
	char* fpath = readString(sock);

	int ret_val = rmdir(fpath);
	writeInt(sock, ret_val);
	if(ret_val!= 0) {
		writeInt(sock, errno);
	}
	//writeInt(sock, rmdir(fpath));
	free(fpath);
	return 1;
}

int makeDir(int sock) {
	char* fpath = readString(sock);
	int mode = (mode_t)readInt(sock);

	int ret_val = mkdir(fpath, mode);
	writeInt(sock, ret_val);
	if(ret_val!= 0) {
		writeInt(sock, errno);
	}
	//writeInt(sock, mkdir(fpath, mode));
	free(fpath);
	return 1;
}

/*int getAttr(int sock) {
	char* fpath = readString(sock);
	struct stat statbuf;
	int dataread = 0;
	int size = sizeof(struct stat);
	
	int ret_val = lstat(fpath, &statbuf);
	writeInt(sock, ret_val);
	if(ret_val != 0) {
		writeInt(sock, errno);
	} else {
		if(mylog) printf("size of stat buf:%d, %d\n", size, sizeof(struct stat));
		send(sock, &statbuf, sizeof(struct stat), MSG_NOSIGNAL);
	}
	while(dataread != size) {
		dataread = dataread + recv(sock, &statbuf, sizeof(struct stat), 0);
		if(dataread < 0) {
			return dataread;//perror("Error reading data\n");
		}
		if(dataread != size) {
			printf("Expected %d, read %d\n", size, dataread);
		}
	}
}*/

int getAttr(int sock) {
	char* fpath = readString(sock);
	struct stat statbuf;
	int dataread = 0;
	int size = sizeof(struct stat);
	
	int ret_val = lstat(fpath, &statbuf);
	writeInt(sock, ret_val);
	if(ret_val != 0) {
		writeInt(sock, errno);
	} else {
		if(mylog) printf("size of stat buf:%d, %d\n", size, sizeof(struct stat));
		writeStatBuf(sock, &statbuf);
	}
	/*while(dataread != size) {
		dataread = dataread + recv(sock, &statbuf, sizeof(struct stat), 0);
		if(dataread < 0) {
			return dataread;//perror("Error reading data\n");
		}
		if(dataread != size) {
			printf("Expected %d, read %d\n", size, dataread);
		}
	}*/
	free(fpath);
}

int fileWrite(int sock) {
	int fileFD = readInt(sock);	//read file descriptor
	char *writeBuf = readString(sock);
	size_t count = (size_t)readInt(sock);	//read the number of bytes to be read
	off_t offset = (off_t)readInt(sock);	//read the offset in the file

	int ret_val = pwrite(fileFD, writeBuf, count, offset);
	writeInt(sock, ret_val);
	if(ret_val < 0) {
		writeInt(sock, errno);
	}
	free(writeBuf);
	return ret_val;
}

int createFile(int sock) {
	char* fpath = readString(sock);
	mode_t mode = (mode_t)readInt(sock);

	int fd = creat(fpath, mode);
	writeInt(sock, fd);
	if(fd < 0) {
		writeInt(sock, errno);
	} else {
		close(fd);
		//writeInt(sock, &statbuf, size, 0);
	}
	free(fpath);
	return 1;
}

int createFileUsingMknod(int sock) {
	char* fpath = readString(sock);
	int flags = readInt(sock);
	mode_t mode = (mode_t)readInt(sock);
	if(mylog) printf("File creation request:%s\n", fpath);

	//int fd = open(fpath, flags, mode);
	int fd = mknod(fpath, mode, 0);
	writeInt(sock, fd);
	if(fd < 0) {
		writeInt(sock, errno);
	} /*else {
		close(fd);
		//writeInt(sock, &statbuf, size, 0);
	}*/
	free(fpath);
	return 1;
}

int deleteFile(int sock) {
	char* fpath = readString(sock);
	int ret_val = unlink(fpath);
	writeInt(sock, ret_val);
	if(ret_val < 0) {
		writeInt(sock, errno);
	}
}

int readDirectory(int sock) {
	int fileFD = readInt(sock);

	DIR *dir;
	struct dirent *dirEntry;
	dir = (DIR*)(uintptr_t)fileFD;

	dirEntry = readdir(dir);
	if(dirEntry == 0){
	}
}

int openDirectory(int sock) {
	char* fpath = readString(sock);

	DIR *dir = opendir(fpath);
	if(dir == NULL) {
	}
	free(fpath);
}

int writeHeader(int sock, char *type, char *path, int writePath) {
	if(mylog) printf("writing header to socket:%d\n", sock);
	int err = writeString(sock, type);
	if(err == -1) return err;
	if(writePath) {
		writeString(sock, path);
	}
}

int writeBytes(int sock, long long field, int size) {
	if(mylog) printf("Wrote :%lld\n", field);

    int len = -1;
    
    unsigned char *p = (char*)&field;
    
    len = send(sock, p, size, MSG_NOSIGNAL);

    if(len == -1) checkErrNo();

    if(mylog) printf("sent %d bytes\n", len);
}

long long readBytes(int s, int size) {
	int dataread = 0;
	long long num;
	unsigned char *p = (char*)&num;

	while(dataread != size) {
		dataread  = dataread + recv(s, &p[dataread], size-dataread, 0);
		if(dataread <= 0) {
			//socket was closed!
			checkErrNo();
			SOCK_CLOSE = 1;
			return ;
		}
		if(mylog && dataread != sizeof(int)) printf("Expected %d, read %d\n", sizeof(int), dataread);
		if(dataread < 0) {
			  perror("Error reading data\n");
		}
	}
	//int val = ntohl(num);
	//int val = stringToInteger(p);
	if(mylog) printf("Read :%lld\n", num);
	return num;
}

int writeStatBuf(int sock, struct stat *si) {
	//  dev_t     st_dev;     /* ID of device containing file */
	//log_struct(si, st_dev, %lld, );
	writeBytes(sock, si->st_dev, sizeof(dev_t));

	//  ino_t     st_ino;     /* inode number */
	//log_struct(si, st_ino, %lld, );
	writeBytes(sock, si->st_ino, sizeof(ino_t));

	//  mode_t    st_mode;    /* protection */
	//log_struct(si, st_mode, 0%o, );
	writeBytes(sock, si->st_mode, sizeof(mode_t));

	//  nlink_t   st_nlink;   /* number of hard links */
	//log_struct(si, st_nlink, %d, );
	writeBytes(sock, si->st_nlink, sizeof(nlink_t));

	//  uid_t     st_uid;     /* user ID of owner */
	//log_struct(si, st_uid, %d, );
	writeBytes(sock, si->st_uid, sizeof(uid_t));

	//  gid_t     st_gid;     /* group ID of owner */
	//log_struct(si, st_gid, %d, );
	writeBytes(sock, si->st_gid, sizeof(gid_t));

	//  dev_t     st_rdev;    /* device ID (if special file) */
	//log_struct(si, st_rdev, %lld,  );
	writeBytes(sock, si->st_rdev, sizeof(dev_t));

	//  off_t     st_size;    /* total size, in bytes */
	//log_struct(si, st_size, %lld,  );
	writeBytes(sock, si->st_size, sizeof(off_t));

	//  blksize_t st_blksize; /* blocksize for filesystem I/O */
	//log_struct(si, st_blksize, %ld,  );
	writeBytes(sock, si->st_blksize, sizeof(blksize_t));

	//  blkcnt_t  st_blocks;  /* number of blocks allocated */
	//log_struct(si, st_blocks, %lld,  );
	writeBytes(sock, si->st_blocks, sizeof(blkcnt_t));

	//  time_t    st_atime;   /* time of last access */
	//log_struct(si, st_atime, 0x%08lx, );
	writeBytes(sock, si->st_atime, sizeof(time_t));

	//  time_t    st_mtime;   /* time of last modification */
	//log_struct(si, st_mtime, 0x%08lx, );
	writeBytes(sock, si->st_mtime, sizeof(time_t));

	//  time_t    st_ctime;   /* time of last status change */
	//log_struct(si, st_ctime, 0x%08lx, );
	writeBytes(sock, si->st_ctime, sizeof(time_t));
}

int readStatBuf(int sock, struct stat *si) {
	si->st_dev = readBytes(sock, sizeof(dev_t));
	si->st_ino = readBytes(sock, sizeof(ino_t));
	si->st_mode = readBytes(sock, sizeof(mode_t));
	si->st_nlink = readBytes(sock, sizeof(nlink_t));
	si->st_uid = readBytes(sock, sizeof(uid_t));
	si->st_gid = readBytes(sock, sizeof(gid_t));
	si->st_rdev = readBytes(sock, sizeof(dev_t));
	si->st_size = readBytes(sock, sizeof(off_t));
	si->st_blksize = readBytes(sock, sizeof(blksize_t));
	si->st_blocks = readBytes(sock, sizeof(blkcnt_t));
	si->st_atime = readBytes(sock, sizeof(time_t));
	si->st_mtime = readBytes(sock, sizeof(time_t));
	si->st_ctime = readBytes(sock, sizeof(time_t));
}
