#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>

enum {
	BUFFERSIZE = 1000,
	MAXLIST = 100,
	READ = 0,
	WRITE = 1,
};

struct tpath {
	char **order;
	char copy[1024];
	int num;
};

struct tcommand {
	int ncommands;
	int *extras;
	char ***ex;
    char **array;
	char **commandpath;
	int status;
};

struct tflags {
    int fpipe;
    int fin;
    int fout;
    int fbg;
    int fenv;
    int fdef;
    char *infile;
    char *outfile;
};

typedef struct tflags tflags;
typedef struct tcommand tcommand;
typedef struct tpath tpath;
int wstatus;

void
clear()
{
    int pid;
    char *clear[2] = {"clear", NULL};
    pid = fork();

    if (pid == -1) {
        fprintf(stderr, "error: fork failed\n");
        wstatus = -1;
    } else if (pid == 0) {
        execv(clear[0], clear);
    } else {
        wait(NULL);
    }
}

// Function to print Current Directory.
// Función para imprimir el directorio actual.
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
searchFlags(char *str, tflags *flags)
{
	char *p;
    int auxin = 0, auxout = 0, auxfg = 0, n = 0;
	flags->fpipe = 0;
    flags->fin = 0;
    flags->fout = 0;
    flags->fbg = 0;
    flags->fenv = 0;
    flags->fdef = 0;
    flags->infile = NULL;
    flags->outfile = NULL;
    
    p = str;

	while (*p != '\0') {
		if (*p == '|') {
			flags->fpipe = 1;
            n++;
		} else if (*p == '<') {
            flags->fin = 1;
            auxin++;
            n++;
        } else if (*p == '>') {
            flags->fout = 1;
            auxout++;
            n++;
        } else if (*p == '&') {
            flags->fout = 1;
            auxfg++;
            n++;
        } else if (*p == '$') {
            flags->fenv = 1;
            n++;
        } else if (*p == '=') {
            flags->fdef = 1;
        }
        p++;
	}
    if (auxin > 1 || auxout > 1 || auxfg > 1) {
        fprintf(stderr, "error: wrong flags\n");
        wstatus = -1;
        return -1;
    }
    if (flags->fdef == 1 && n > 0) {
        fprintf(stderr, "error: cannot define variable\n");
        wstatus = -1;
        return -1;
    }
    return 0;
}

int
parseRedir(char *str, tflags *flags)
{
	char *token, *saveptr;
    
    if (flags->fout == 1) {
        token = strtok_r(str, ">", &saveptr);
        if (token == NULL) {
            fprintf(stderr, "error: redirection in wrong place\n");
            wstatus = -1;
            return -1;
        }
        flags->outfile = malloc(sizeof(char) * strlen(saveptr) +1);
        strcpy(flags->outfile, saveptr);
        str = token; 
    }

    if (flags->fin == 1) {
        token = strtok_r(str, "<", &saveptr); 
        if (token == NULL) {
            fprintf(stderr, "error: redirection in wrong place\n");
            wstatus = -1;
            return -1;
        }
        flags->infile = malloc(sizeof(char) * strlen(saveptr) +1);  
        strcpy(flags->infile, saveptr);
        str = token; 
    }
    return 0;
}

// Función para contar el número de rutas en la variable de entorno PATH.
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

// Función para contar el número de tuberías en una cadena de entrada.
int
countPipes(char *str)
{
    char *p;
    int pipes = 1;
    p = str;

    // Verifica si el primer carácter de la cadena es una tubería.
    if (*p == '|') {
        wstatus = -1;
        fprintf(stderr, "error: using '|' at the beginning\n");
        return -1;
    }

    // Cuenta el número de tuberías en la cadena.
    while (*p != '\0') {
        if (*p == '|') {
            if (*(p+1) == '|' || *(p+1) == '\0') {
                wstatus = -1;
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
splitPath(char *PATH, tpath *paths)
{
    char *token;
    int i = 0;

    paths->order = malloc(sizeof(char*) * paths->num);

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
parsePipe(char *inputString, tcommand *instructions)
{
    char *token;
    int i = 0;

    instructions->array = malloc(sizeof(char*) * instructions->ncommands);

    token = strtok(inputString, "|");

    while (token != NULL && i < instructions->ncommands) {
        instructions->array[i] = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(instructions->array[i],token);
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
		if (*p == ' ' && *(p+1) != ' ' && *(p+1) != '\0') {
			words++;
		}
        p++;
	}

	return words;
}

int
replace_env_vars(tcommand *instructions, int num, char *str, int n)
{
    char *name;
    char *value;
    
    name = str+1;
    printf("EX:%s\n", name);
    value = getenv(name);
    if (value == NULL) {
        wstatus = -1;
        return -1;
    }
    instructions->ex[num][n] = malloc(sizeof(char) * strlen(value) + 1);
    strcpy(instructions->ex[num][n],value);
    printf("AQUI: %s\n", str);
    

    return 0;
}

int parseSpace(tcommand *instructions, int num)
{
    char *token;
    int i = 0;
    instructions->ex[num] = malloc(sizeof(char*) * (instructions->extras[num]+1));

    token = strtok(instructions->array[num], " ");
    if (*token == '$'){
        if (replace_env_vars(instructions, num, token, i) == -1) {
            return -1;
        }
    } else {
        instructions->ex[num][i] = malloc(sizeof(char) * strlen(token) + 1);
        strcpy(instructions->ex[num][i],token);
    }
    for(i = 1; i < instructions->extras[num]; i++){
        token = strtok(NULL, " ");
        if (token != NULL && *token != ' ') {
            if (*token == '$'){
                if (replace_env_vars(instructions, num, token, i) == -1) {
                    return -1;
                }
            } else {
                instructions->ex[num][i] = malloc(sizeof(char) * strlen(token) + 1);
                strcpy(instructions->ex[num][i],token);
            }
        }
    }
    instructions->ex[num][instructions->extras[num]] = NULL;
    return 0;
}


int find_command_path(char **command, tpath paths)
{
    int i;
    char *command_path;

    for (i = 0; i < paths.num; i++) {
        // Asignar memoria para command_path
        command_path = malloc(strlen(paths.order[i]) + strlen(*command) + 1);
        if (command_path == NULL) {
            // Manejar error de asignación de memoria
            return -1;
        }
        strcpy(command_path, paths.order[i]);
        strcat(command_path, *command);
        printf("EJEMPLO: %s\n", command_path);
        if (access(command_path, X_OK) == 0) {
            // Asignar memoria para el nuevo valor de command
            char* new_command = malloc(strlen(command_path) + 1);
            if (new_command == NULL) {
                // Manejar error de asignación de memoria
                free(command_path); // Liberar la memoria asignada a command_path
                fprintf(stderr, "error al asignar memoria\n");
                return -1;
            }
            strcpy(new_command, command_path);
            free(*command); // Liberar la memoria asignada previamente a command
            *command = new_command; // Actualizar el puntero command con la nueva asignación
            free(command_path);
            return 0;
        }
        // Liberar la memoria asignada a command_path
        free(command_path);
    }
    fprintf(stderr, "error: command not found\n");
    return -1;
}

int
firstpipe(int *fd, char **ex)
{
	int pid;

	if (pipe(fd) < 0) {
		fprintf(stderr, "cannot make a pipe\n");
        return -1;
	}

	pid = fork();

	if (pid == 0) {
		close(fd[READ]);

		if (dup2(fd[WRITE], 1) < 0) {
			fprintf(stderr, "dup failed\n");
            return -1;
        }
		close(fd[WRITE]);
		if (execv(ex[0], ex) < 0) {
			fprintf(stderr, "execv failed\n");
            return -1;
        }
	} else if (pid == -1) {
		fprintf(stderr, "cannot fork\n");
        return -1;
	} else {
		close(fd[WRITE]);
	}
    return 0;
}

int
pipes(int *fd1, int *fd2, char **ex)
{
	int pid;

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
	}
    return 0;
}

int
lastpipe(int *fd, char **ex)
{
	int pid;

	pid = fork();

	if (pid == 0) {
		if (dup2(fd[READ], 0) < 0) {
			fprintf(stderr, "dup failed\n");
            return -1;
		}
		close(fd[READ]);
		if (execv(ex[0], ex) < 0) {
			fprintf(stderr, "execv failed\n");
            return -1;
		}
	} else if (pid == -1) {
		fprintf(stderr, "cannot fork\n");
        return -1;
	} else {
		close(fd[READ]);
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
    if (name[strlen(name)-1] == ' ') {
        name[strlen(name)-1] = '\0';
    }
    value = strtok_r(NULL, del, &saveptr);
    if (value[0] == ' ') {
        value++;
    }
    if (name == NULL || value == NULL) {
        return -1;
        wstatus = -1;
    }

    if (setenv(name, value, 1) < 0) {
        return -1;
        wstatus = -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {

    if (argc != 1) {
        exit(EXIT_FAILURE);
    }

    char input[BUFFERSIZE];
    tpath paths;
    tcommand instructions;
    tflags flags;
    int i, j;
    int fd[2], prev_fd[2];
    wstatus = 0;

    //clear();
   
    while (wstatus == 0) {
        paths.num = countDots(getenv("PATH"));
        if (paths.num == -1) {
            exit(EXIT_FAILURE);
        }
        if (splitPath(getenv("PATH"), &paths) == -1) {
            exit(EXIT_FAILURE);
        }
        
        /*printf("$PATH:\n");
        for (i = 0; i < paths.num; i++) {
            fprintf(stderr, "%d: %s\n", i, paths.order[i]);
        }*/
        
        if (printDir() == -1) {
            exit(EXIT_FAILURE);
        }
        if (fgets(input, BUFFERSIZE, stdin) == NULL) {
            fprintf(stderr, "error: cannot read stdin\n");
            exit(EXIT_FAILURE);
        }
        input[strlen(input) -1] = '\0';

        // Check for exit command
        if (strcmp(input, "exit") == 0) {
            for (i = 0; i < paths.num; i++) {
                free(paths.order[i]);
            }
            free(paths.order);
            exit(0);
        }

        if (searchFlags(input, &flags) == -1) {
            exit(EXIT_FAILURE);
        }
        if (parseRedir(input, &flags) == -1) {
            exit(EXIT_FAILURE);
        }
        if (flags.fdef == 1) {
            if (defvars(input) == -1) {
                exit(EXIT_FAILURE);
            }
        } else {
            instructions.ncommands = countPipes(input);
            if (instructions.ncommands == -1) {
                exit(EXIT_FAILURE);
            }
            printf("Numero de comandos: %d\n", instructions.ncommands);
            
            instructions.extras = malloc(sizeof(int) * instructions.ncommands);
            instructions.ex = malloc(sizeof(char**) * instructions.ncommands);

            if (parsePipe(input, &instructions) == -1) {
                exit(EXIT_FAILURE);
            }

            for (i = 0; i < instructions.ncommands; i++) {
                instructions.extras[i] = countWords(instructions.array[i]);
                if (instructions.extras[i] == -1) {
                    exit(EXIT_FAILURE);
                }
                printf("Comando sin parsear: %s\n", instructions.array[i]);
                if (parseSpace(&instructions, i) == -1) {
                    exit(EXIT_FAILURE);
                }
                if (find_command_path(&instructions.ex[i][0], paths) == -1) {
                    exit(EXIT_FAILURE);
                }
                printf("Comando parseado %d con %d extras es: ", i, instructions.extras[i]);
                for (j = 0; j < instructions.extras[i]; j++) {
                    printf("%s\n", instructions.ex[i][j]);
                }
            }
            printf("Redirecciones: IN: %s, OUT: %s\n", flags.infile, flags.outfile);
            /* Ejecutar el primer comando */
            if (firstpipe(fd, instructions.ex[0]) == -1) {
                exit(EXIT_FAILURE);
            }
            memcpy(prev_fd, fd, sizeof(fd));

            for (i = 1; i < instructions.ncommands-1; i++) {
                /* Ejecutar comandos intermedios */
                if (pipes(prev_fd, fd, instructions.ex[i]) == -1) {
                    exit(EXIT_FAILURE);
                }
                memcpy(prev_fd, fd, sizeof(fd));
            }
            /* Ejecutar el último comando */
            if (lastpipe(prev_fd, instructions.ex[instructions.ncommands-1]) == -1) {
                exit(EXIT_FAILURE);
            }
            /* Esperar a que terminen todos los procesos */
            while (wait(NULL) > 0);
            for (i = 0; i < instructions.ncommands; i++) {
                free(instructions.array[i]);
                for (j = 0; j < instructions.extras[i]; j++) {
                    free(instructions.ex[i][j]);
                }
                free(instructions.ex[i]);
            }
            free(instructions.ex);
            free(instructions.array);
            free(instructions.extras);
        }
        
        /*Liberar memoria*/
        if (flags.fin == 1) {
            free(flags.infile);
        }
        if (flags.fout == 1) {
            free(flags.outfile);
        }
        for (i = 0; i < paths.num; i++) {
            free(paths.order[i]);
        }
        free(paths.order);

    }
	exit(0);
}