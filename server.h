enum MSGTYPE  {
	REGISTER = 0,
	OPEN,
	MKDIR,
	RMDIR,
	READ,
	WRITE,
	GETATTR
}

typedef struct list* LIST;
struct list {
	void* data;
	void* next;
};
