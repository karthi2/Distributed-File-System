// There are a couple of symbols that need to be #defined before
// #including all the headers.

#ifndef _PARAMS_H_
#define _PARAMS_H_

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500

// maintain bbfs state in here
#include <limits.h>
#include <stdio.h>
#include <pthread.h>

/*struct my_state {
    FILE *logfile;
    char *rootdir;
};
#define BB_DATA ((struct my_state *) fuse_get_context()->private_data)*/

typedef struct Message* MESSAGE;
struct Message {
	char *operation;
	char *fname;
};

typedef enum msgtype MSGTYPE;
enum msgtype {
	REGISTER = 0,
	CREAT,
	OPEN,
	MKDIR,
	RMDIR,
	CLOSE,
	READ,
	WRITE,
	GETATTR,
	READDIR,
	OPENDIR,
	SYNC,
	NOTHING,
	MKNOD,
	UNLINK,
	REINSTATE,
	FAILEDSERVER,
	RELEASE
};

typedef struct threadlist* THREADLIST;
struct threadlist {
	pthread_t thread;
	THREADLIST next;
};

typedef struct list* LIST;
struct list {
	void *data;
	void *next;
};

#endif
