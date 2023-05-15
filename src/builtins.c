#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include "util.h"
#include "builtins.h"
#include "tokenizer.h"

int
printDir()
{
	char cwd[1024];
	char *username = getenv("USER");

	if (username == NULL) {
		return -1;
	}
	getcwd(cwd, sizeof(cwd));
	printf("%s:~%s$ ", username, cwd);
	return 0;
}

int
change_directory(char *path)
{
	char cwd[1024];

	if (clear_spaces(path) < 0) {
		return -1;
	}
	if (path == NULL) {
		strcpy(cwd, "/home");
	} else if (path[0] != '/') {
		// Si el path es relativo, concatenar con el path actual
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr,
				"error: unable to get current directory\n");
			return -1;
		}
		strcat(cwd, "/");
		strcat(cwd, path);
	} else {
		strcpy(cwd, path);
	}
	if (chdir(cwd) < 0) {
		fprintf(stderr, "error: unable to change directory to %s\n",
			cwd);
		return -1;
	}
	return 0;
}
