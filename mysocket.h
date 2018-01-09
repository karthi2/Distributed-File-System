#include "params.h"

//typedef struct sockaddr_in *SOCKADDR_IN;

int checkErrNo();
int checkErrNoHostName();

int integerToString(unsigned int,unsigned char *);
int stringToInteger(unsigned char *);

int my_log(char *);
int printBuffer(unsigned char *);

int openServerSocket(int, struct sockaddr_in*);

int connect_server(char *, int, int *, struct sockaddr_in*);

int readByteByByte(int, unsigned char *);

int writeInt(int, int);
int readInt(int);

int writeString(int, char *);
char* readString(int);

int writeMSG(int , char* , int , int , int* , int );
int readMSG(int , char* , int* , int* , int** );

MESSAGE createMessage(char*, char*);
MSGTYPE getType(char*);

int fileopen(int);
int fileclose(int);

int fileread(int);
int fileWrite(int);
int createFile(int);

int writeHeader(int, char *, char *, int);
char* readHeader(int);

int removeDir(int);
int makeDir(int);

int getAttr(int);

int readDirectory(int);
int openDirectory(int);
