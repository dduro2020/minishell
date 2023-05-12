#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include "util.h"
#include "tokenizer.h"

void
clear()
{
	printf("\033[H\033[J");
}

int
clear_spaces(char *str)
{

	int j;
	int i = 0;

	if (str != NULL) {
		while (str[0] == ' ') {
			for (j = 0; j < strlen(str); j++) {
				str[j] = str[j + 1];
			}
		}
		while (str[i] != ' ' && str[i] != '\0' && i < strlen(str)) {
			i++;
		}

		str[i] = '\0';
	}

	return 0;
}

int
searchFlags(char *str, tflags * flags)
{
	char *p;
	int auxin = 0, auxout = 0, auxfg = 0, n = 0, auxeq = 0;

	flags->fpipe = 0;
	flags->fin = 0;
	flags->fout = 0;
	flags->fbg = 0;
	flags->fenv = 0;
	flags->fdef = 0;
	flags->firstout = 0;
	flags->firstin = 0;
	flags->infile = NULL;
	flags->outfile = NULL;

	p = str;

	while (*p != '\0') {
		if (*p == '|') {
			flags->fpipe = 1;
			n++;
		} else if (*p == '<') {
			if (flags->fout == 0) {
				flags->firstin = 1;
			}
			flags->fin = 1;
			auxin++;
			n++;
		} else if (*p == '>') {
			if (flags->fin == 0) {
				flags->firstout = 1;
			}
			flags->fout = 1;
			auxout++;
			n++;
		} else if (*p == '&') {
			flags->fbg = 1;
			auxfg++;
			n++;
		} else if (*p == '$') {
			flags->fenv = 1;
			n++;
		} else if (*p == '=') {
			flags->fdef = 1;
			auxeq++;
		}
		p++;
	}
	if (auxin > 1 || auxout > 1 || auxfg > 1 || auxeq > 1) {
		fprintf(stderr, "error: wrong flags\n");
		return -1;
	}
	if (flags->fdef == 1 && n > 0) {
		fprintf(stderr, "error: cannot define variable\n");
		return -1;
	}

	return 0;
}

int
parseRedir_out(char *str, tflags * flags)
{
	char *token, *saveptr;

	if (flags->fout == 1) {
		token = strtok_r(str, ">", &saveptr);
		if (token == NULL) {
			fprintf(stderr, "error: redirection in wrong place\n");
			return -1;
		}
		if (clear_spaces(saveptr) < 0) {
			return -1;
		}
		flags->outfile = malloc(sizeof(char) * strlen(saveptr) + 1);
		strcpy(flags->outfile, saveptr);
		str = token;
	} else if (flags->fbg == 1) {
		flags->outfile = malloc(sizeof(char) * strlen("/dev/null") + 1);
		strcpy(flags->outfile, "/dev/null");
	}

	return 0;
}

int
parseRedir_in(char *str, tflags * flags)
{
	char *token, *saveptr;

	if (flags->fin == 1) {
		token = strtok_r(str, "<", &saveptr);
		if (token == NULL) {
			fprintf(stderr, "error: redirection in wrong place\n");
			return -1;
		}
		if (clear_spaces(saveptr) < 0) {
			return -1;
		}
		flags->infile = malloc(sizeof(char) * strlen(saveptr) + 1);
		strcpy(flags->infile, saveptr);
		str = token;
	}

	return 0;
}

int
parseRedir(char *str, tflags * flags)
{
	if (flags->firstout == 1) {
		if (parseRedir_in(str, flags) < 0) {
			return -1;
		}
		if (parseRedir_out(str, flags) < 0) {
			return -1;
		}
	} else {
		if (parseRedir_out(str, flags) < 0) {
			return -1;
		}
		if (parseRedir_in(str, flags) < 0) {
			return -1;
		}
	}

	return 0;
}

int
parseBg(char *str, tflags * flags)
{
	char *token, *saveptr;

	if (flags->fbg == 1) {
		token = strtok_r(str, "&", &saveptr);
		if (token == NULL) {
			fprintf(stderr, "error: background in wrong place\n");
			return -1;
		}
		str = token;
	}
	return 0;
}

int
countDots(char *PATH)
{
	char *p;
	int dots = 1;

	p = PATH;
	while (*p != '\0') {
		if (*p == ':') {
			dots++;
		}
		p++;
	}

	return dots;
}

int
countPipes(char *str)
{
	char *p;
	int pipes = 1;

	p = str;

	// Verifica si el primer carácter de la cadena es una tubería.
	if (*p == '|') {
		fprintf(stderr, "error: using '|' at the beginning\n");
		return -1;
	}
	// Cuenta el número de tuberías en la cadena.
	while (*p != '\0') {
		if (*p == '|') {
			if (*(p + 1) == '|' || *(p + 1) == '\0') {
				fprintf(stderr, "error: wrong use of '|'\n");
				return -1;
			} else {
				pipes++;
			}
		}
		p++;
	}

	return pipes;
}

int
splitPath(char *PATH, tpath * paths)
{
	char *token;
	int i = 0;

	paths->order = malloc(sizeof(char *) * paths->num);

	strncpy(paths->copy, PATH, strlen(PATH) + 1);

	token = strtok(paths->copy, ":");

	while (token != NULL) {
		paths->order[i] = malloc(sizeof(char) * (strlen(token) + 2));
		strcpy(paths->order[i], token);
		strcat(paths->order[i], "/");
		i++;
		token = strtok(NULL, ":");
	}

	return 0;
}

int
parsePipe(char *inputString, tcommand * instructions)
{
	char *token;
	int i = 0;

	instructions->array = malloc(sizeof(char *) * instructions->ncommands);

	token = strtok(inputString, "|");

	while (token != NULL && i < instructions->ncommands) {
		instructions->array[i] =
		    malloc(sizeof(char) * (strlen(token) + 1));
		strcpy(instructions->array[i], token);
		i++;
		token = strtok(NULL, "|");
	}
	return 0;
}

int
countWords(char *str)
{
	char *p;
	int words = 1;

	p = str;

	/* comprobar si la cadena comienza por espacios */
	while (*p == ' ') {
		p++;
	}

	while (*p != '\0') {
		if (*p == ' ' && *(p + 1) != ' ' && *(p + 1) != '\0') {
			words++;
		}
		p++;
	}

	return words;
}

int
replace_env_vars(tcommand * instructions, int num, char *str, int n)
{
	char *value;

	if (clear_spaces(str) < 0) {
		return -1;
	}
	str = str + 1;
	value = getenv(str);
	if (value == NULL) {
		fprintf(stderr, "error: no coincidences with :$%s\n", str);
		return -1;
	}
	instructions->ex[num][n] = malloc(sizeof(char) * strlen(value) + 1);
	strcpy(instructions->ex[num][n], value);

	return 0;
}

int
parseSpace(tcommand * instructions, int num)
{
	char *token;
	int i = 0;

	instructions->ex[num] =
	    malloc(sizeof(char *) * (instructions->extras[num] + 1));

	token = strtok(instructions->array[num], " ");
	if (*token == '$') {
		if (replace_env_vars(instructions, num, token, i) == -1) {
			return -1;
		}
	} else {
		instructions->ex[num][i] =
		    malloc(sizeof(char) * strlen(token) + 1);
		strcpy(instructions->ex[num][i], token);
	}
	for (i = 1; i < instructions->extras[num]; i++) {
		token = strtok(NULL, " ");
		if (token != NULL && *token != ' ') {
			if (*token == '$') {
				if (replace_env_vars
				    (instructions, num, token, i) == -1) {
					return -1;
				}
			} else {
				instructions->ex[num][i] =
				    malloc(sizeof(char) * strlen(token) + 1);
				strcpy(instructions->ex[num][i], token);
			}
		}
	}
	instructions->ex[num][instructions->extras[num]] = NULL;
	return 0;
}

int
find_command_path(char **command, tpath paths)
{
	int i;
	char *command_path;

	if (*command[0] == '.') {
		return 0;
	} else {

		for (i = 0; i < paths.num; i++) {
			// Asignar memoria para command_path
			command_path =
			    malloc(strlen(paths.order[i]) + strlen(*command) +
				   1);
			if (command_path == NULL) {
				// Manejar error de asignación de memoria
				return -1;
			}
			strcpy(command_path, paths.order[i]);
			strcat(command_path, *command);
			if (access(command_path, X_OK) == 0) {
				// Asignar memoria para el nuevo valor de command
				char *new_command =
				    malloc(strlen(command_path) + 1);
				if (new_command == NULL) {
					// Manejar error de asignación de memoria
					free(command_path);	// Liberar la memoria asignada a command_path
					fprintf(stderr,
						"error al asignar memoria\n");
					return -1;
				}
				strcpy(new_command, command_path);
				free(*command);	// Liberar la memoria asignada previamente a command
				*command = new_command;	// Actualizar el puntero command con la nueva asignación
				free(command_path);
				return 0;
			}
			// Liberar la memoria asignada a command_path
			free(command_path);
		}
	}
	fprintf(stderr, "error: command not found\n");
	return -1;
}
