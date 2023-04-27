#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include <fcntl.h>

enum {
	BUFFERSIZE = 1000,
	MAXLIST = 100,
	READ = 0,
	WRITE = 1,
};

typedef struct {
	int wstatus;
}tstatus;

typedef struct {
	char **order;
	char copy[1024];
	int num;
}tpath;

typedef struct {
	int ncoms;
	int *extras;
	char ***ex;
    char **array;
	char **commandpath;
	int status;
}tcommand;

void printDir();
void clear();
void openHelp();
int countDots(char *PATH);
int countPipes(char *str, tstatus *status);
int  countWords(char *str);
void splitPath(char *PATH, tpath *paths);
void parseRedir(char *inputString, tcommand *instructions, char *flags, int f);
void parsePipe(char *inputString, tcommand *instructions);
void parseSpace(tcommand *instructions, int num);
void executeCommand(tpath *paths, tcommand *instructions, char *command, int num, int i);
void pipes(tcommand *instructions, tpath *paths);
void redirections(tcommand *instructions, tpath *paths, char *flags, int f);