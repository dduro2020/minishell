#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include "util.h"
#include "tokenizer.h"
#include "executor.h"
#include "builtins.h"

int
main(int argc, char *argv[])
{

	if (argc != 1) {
		exit(EXIT_FAILURE);
	}

	char input[BUFFERSIZE];
	tpath paths;
	tcommand instructions;
	tflags flags;
	int i, j, in_file, out_file, auxstatus;
	int fd[2], prev_fd[2];
	int max_buf = 0;


	clear();

	while (1) {
		in_file = -1;
		out_file = -1;
		auxstatus = 0;

		paths.num = countDots(getenv("PATH"));
		if (paths.num == -1) {
			exit(EXIT_FAILURE);
		}
		if (splitPath(getenv("PATH"), &paths) == -1) {
			exit(EXIT_FAILURE);
		}
		if (printDir() == -1) {
			exit(EXIT_FAILURE);
		}
		if (fgets(input, BUFFERSIZE, stdin) == NULL) {
			fprintf(stderr, "error: cannot read stdin\n");
			exit(EXIT_FAILURE);
		}

		if ( input[strlen(input) - 1] != '\n') {
			max_buf = 1;
			fprintf(stderr, "error: exceeded buffer length\n");
			auxstatus = -1;
		} else {
			if ( max_buf == 1 ) {
				fprintf(stderr, "error: cleaning buffer...\n");
				max_buf = 0;
				auxstatus = -1;
			}
		}

		input[strlen(input) - 1] = '\0';
		
		if (check_input(input) < 0) {
			exit(EXIT_FAILURE);
		}
		// Check for exit command
		if (strcmp(input, "exit") == 0) {
			for (i = 0; i < paths.num; i++) {
				free(paths.order[i]);
			}
			free(paths.order);
			exit(EXIT_SUCCESS);
		}

		if (strcmp(input, "\0") != 0 && auxstatus == 0 ) {
			if (searchFlags(input, &flags) == -1) {
				fprintf(stderr, "error: wrong flags\n");
				auxstatus = -1;
				//exit(EXIT_FAILURE);
			}
		
			if (auxstatus == 0) {
				if (parseRedir(input, &flags) == -1) {
					auxstatus = -1;
					//exit(EXIT_FAILURE);
				}
				if (parseBg(input, &flags) == -1) {
					auxstatus = -1;
					//exit(EXIT_FAILURE);
				}
				if (flags.fdef == 1) {
					if (defvars(input) == -1) {
						exit(EXIT_FAILURE);
					}
				} else {
					instructions.ncommands =
						countPipes(input);
					if (instructions.ncommands == -1) {
						exit(EXIT_FAILURE);
					}

					instructions.extras =
						malloc(sizeof(int) *
							instructions.ncommands);
					instructions.ex =
						malloc(sizeof(char **) *
							instructions.ncommands);

					if (parsePipe(input, &instructions) ==
						-1) {
						exit(EXIT_FAILURE);
					}

					for (i = 0; i < instructions.ncommands;
							i++) {
						instructions.extras[i] =
							countWords(instructions.
									array[i]);
						if (instructions.extras[i] ==
							-1) {
							exit(EXIT_FAILURE);
						}
						if (parseSpace(&instructions, i)
							== -1) {
							auxstatus = -1;
						}
						if (strcmp
							(instructions.ex[0][0],
								"cd") == 0) {
							break;
						}
						if (find_command_path
							(&instructions.ex[i][0],
								paths) == -1) {
							auxstatus = -1;
						}
					}
					if (strcmp(instructions.ex[0][0], "cd")
						== 0) {
						if (instructions.extras[0] > 2) {
							fprintf(stderr,
								"error: cd [dir]\n");
						} else if (instructions.
								extras[0] == 1) {
							change_directory(NULL);
						} else {
							change_directory
								(instructions.
									ex[0][1]);
						}
					} else if (auxstatus == 0){
						/* Ejecutar el primer comando */
						if (flags.fin == 1) {
							in_file =
								handleRedir(flags.
									infile,
									O_RDONLY,
									S_IRUSR);
						}
						if (flags.fout == 1) {
							out_file =
								handleRedir(flags.
									outfile,
									O_WRONLY
									|
									O_CREAT
									|
									O_TRUNC,
									S_IWUSR
									|
									S_IRUSR);
						} else if (flags.fbg == 1) {
							in_file =
								handleRedir(flags.
									infile,
									O_WRONLY
									|
									O_CREAT
									|
									O_TRUNC,
									S_IWUSR
									|
									S_IRUSR);
						}
						if (firstpipe
							(fd, instructions.ex[0],
								in_file, out_file,
								instructions.ncommands, flags.fbg) ==
							-1) {
							exit(EXIT_FAILURE);
						}
						memcpy(prev_fd, fd, sizeof(fd));

						for (i = 1;
								i <
								instructions.ncommands - 1;
								i++) {
							/* Ejecutar comandos intermedios */
							if (pipes
								(prev_fd, fd,
									instructions.
									ex[i], flags.fbg) == -1) {
								exit(EXIT_FAILURE);
							}
							memcpy(prev_fd, fd,
									sizeof(fd));
						}
						/* Ejecutar el último comando */
						if (instructions.ncommands > 1) {
							if (lastpipe
								(prev_fd,
									instructions.
									ex[instructions.
								ncommands - 1],
									out_file, flags.fbg) == -1) {
								exit(EXIT_FAILURE);
							}
						}
					}
				}

				/*Liberar memoria */
				if (flags.fdef == 0) {
					for (i = 0; i < instructions.ncommands;
							i++) {
						free(instructions.array[i]);
						for (j = 0;
								j < instructions.extras[i];
								j++) {
							free(instructions.
									ex[i][j]);
						}
						free(instructions.ex[i]);
					}
					free(instructions.ex);
					free(instructions.array);
					free(instructions.extras);
				}

				if (flags.fin == 1 || flags.fbg == 1) {
					if (auxstatus == 0) {
						close(in_file);
					}
					free(flags.infile);
				}
				if (flags.fout == 1) {
					if (auxstatus == 0) {
						close(out_file);
					}
					free(flags.outfile);
				}
			}
		}

		for (i = 0; i < paths.num; i++) {
			free(paths.order[i]);
		}
		free(paths.order);
	}
	exit(0);
}
