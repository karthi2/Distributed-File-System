#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define NFILES 300

int main() {
	int fd[NFILES];
	int i = 0;
	char *filename = (char *) malloc (sizeof(char)*15);
	for(i=1; i<=NFILES; i++) {
		sprintf(filename, "/tmp/fusemount/%d", i);
		fd[i] = open(filename, O_RDWR|O_CREAT, 0644);
		if(fd[i] < 0) {
			printf("Error opening the file %d\n", i);
			continue;
		}
		printf("Successfully created the file %d\n", i);
		close(fd[i]);
	}
	for(i=1; i<=NFILES; i++) {
		char *buf = (char *) malloc (sizeof(char)*15);;
		strcpy(buf, "Hello World");
		buf[12]='\0';
		sprintf(filename, "/tmp/fusemount/%d", i);
		fd[i] = open(filename, O_WRONLY);
		if(fd[i] < 0) {
			printf("Error opening the file %d for writing\n", i);
			continue;
		}
			
		int rb = write(fd[i], buf, strlen(buf));
		if(rb < 0) {
			printf("writing failed to file %d\n", i);
			continue;
		}
		close(fd[i]);
		free(buf);
	}
    	//free(buf);
	sleep(5);
	for(i=1; i<=NFILES; i++) {
		sprintf(filename, "/tmp/fusemount/%d", i);
    		fd[i] = open(filename, O_RDONLY);
		if(fd[i] < 0) {
			printf("Error opening the file %d for reading\n", i);
			continue;
		}
		char *readbuf = malloc(15);
		int rb = read(fd[i], readbuf, 11);
		if(rb < 0) {
			printf("reading failed from  file %d\n", i);
			continue;
		}
		printf("read = %d bytes  buf = %s\n", rb, readbuf);
		close(fd[i]);
	}

	/*int ret_val = unlink("/home/avinash/FuseTest/hello1.txt");
	if(ret_val < 0) {
		perror("unlink failed\n");
 		exit(0);
	}*/
}
