#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include "util.h"
#include "executor.h"
#include "tokenizer.h"

int
firstpipe(int *fd, char **ex, int in_file, int out_file, int n)
{
	int pid, auxstatus;

	if (pipe(fd) < 0) {
		fprintf(stderr, "cannot make a pipe\n");
		return -1;
	}

	pid = fork();
	if (pid == 0) {
		close(fd[READ]);
		if (in_file != -1) {
			if (dup2(in_file, 0) < 0) {
				fprintf(stderr, "dup failed\n");
				return -1;
			}
			close(in_file);
		}

		if (n == 1 && out_file != -1) {
			if (dup2(out_file, 1) < 0) {
				fprintf(stderr, "dup failed\n");
				return -1;
			}
			close(out_file);
			close(fd[WRITE]);
		} else if (n == 1 && out_file == -1) {
			close(fd[WRITE]);
		} else {
			if (dup2(fd[WRITE], 1) < 0) {
				fprintf(stderr, "dup failed\n");
				return -1;
			}
			close(fd[WRITE]);
		}
		if (execv(ex[0], ex) < 0) {
			fprintf(stderr, "execv failed\n");
			return -1;
		}
	} else if (pid == -1) {
		fprintf(stderr, "cannot fork\n");
		return -1;
	} else {
		close(fd[WRITE]);
		if (n == 1 && out_file != -1) {
			close(out_file);
		}
		if (in_file != -1) {
			close(fd[READ]);
			close(in_file);
		}
		wait(&auxstatus);
	}
	return 0;
}

int
pipes(int *fd1, int *fd2, char **ex)
{
	int pid, auxstatus;

	if (pipe(fd2) < 0) {
		fprintf(stderr, "cannot make a pipe\n");
		return -1;
	}
	pid = fork();

	if (pid == 0) {
		close(fd2[READ]);

		if (dup2(fd1[READ], 0) < 0) {
			fprintf(stderr, "dup failed\n");
			return -1;
		}
		close(fd1[READ]);

		if (dup2(fd2[WRITE], 1) < 0) {
			fprintf(stderr, "dup failed\n");
			return -1;
		}
		close(fd2[WRITE]);

		if (execv(ex[0], ex) < 0) {
			fprintf(stderr, "execv failed\n");
			return -1;
		}

	} else if (pid == -1) {
		fprintf(stderr, "cannot fork\n");
		return -1;
	} else {
		close(fd1[READ]);
		close(fd2[WRITE]);
		wait(&auxstatus);
	}
	return 0;
}

int
lastpipe(int *fd, char **ex, int out_file)
{
	int pid, auxstatus;

	pid = fork();

	if (pid == 0) {
		if (dup2(fd[READ], 0) < 0) {
			fprintf(stderr, "dup failed\n");
			return -1;
		}
		close(fd[READ]);
		if (out_file != -1) {
			if (dup2(out_file, 1) < 0) {
				fprintf(stderr, "dup failed\n");
				return -1;
			}
			close(out_file);
		}

		if (execv(ex[0], ex) < 0) {
			fprintf(stderr, "execv failed\n");
			return -1;
		}
	} else if (pid == -1) {
		fprintf(stderr, "cannot fork\n");
		return -1;
	} else {
		close(fd[READ]);
		wait(&auxstatus);
	}
	return 0;
}

int
defvars(char *inputString)
{
	char *name;
	char *value;
	char *saveptr;
	char del[1] = "=";

	name = strtok_r(inputString, del, &saveptr);
	if (clear_spaces(name) < 0) {
		return -1;
	}
	value = strtok_r(NULL, del, &saveptr);
	if (clear_spaces(value) < 0) {
		return -1;
	}
	if (name == NULL || value == NULL) {
		return -1;
	}

	if (setenv(name, value, 1) < 0) {
		return -1;
	}

	return 0;
}

int
handleRedir(char *file, int fflags, int mode)
{
	int fd;

	fd = open(file, fflags, mode);
	if (fd < 0) {
		fprintf(stderr, "error: cannot open file\n");
		exit(EXIT_FAILURE);
	}
	return fd;
}
