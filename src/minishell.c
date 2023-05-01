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
    int firstout;
    int firstin;
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
    printf("\033[H\033[J");
}

int
clear_spaces(char *str) {
    
    int j;
    int i = 0;
    if (str != NULL) {
        while (str[0] == ' ') {
            for (j = 0; j < strlen(str); j++) {
                str[j] = str[j+1];
            }
        }
        while (str[i] != ' ' && str[i] != '\0' && i < strlen(str)) {
            i++;
        }
        
        str[i] = '\0';
    }

    return 0;
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

int change_directory(char* path) {
    char cwd[1024];
    if (clear_spaces(path) < 0) {
        return -1;
    }
    if (path == NULL) {
        strcpy(cwd, "/home");
    } else if (path[0] != '/') {
        // Si el path es relativo, concatenar con el path actual
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "error: unable to get current directory\n");
            return -1;
        }
        strcat(cwd, "/");
        strcat(cwd, path);
    } else {
        strcpy(cwd, path);
    }
    if (chdir(cwd) < 0) {
        fprintf(stderr, "error: unable to change directory to %s\n", cwd);
        return -1;
    }
    return 0;
}

int
searchFlags(char *str, tflags *flags)
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
parseRedir_out(char *str, tflags *flags)
{
	char *token, *saveptr;
    
    if (flags->fout == 1) {
        token = strtok_r(str, ">", &saveptr);
        if (token == NULL) {
            fprintf(stderr, "error: redirection in wrong place\n");
            wstatus = -1;
            return -1;
        }
        if (clear_spaces(saveptr) < 0) {
            wstatus = -1;
            return -1;
        }
        flags->outfile = malloc(sizeof(char) * strlen(saveptr) +1);
        strcpy(flags->outfile, saveptr);
        str = token; 
    } else if (flags->fbg == 1) {
        flags->outfile = malloc(sizeof(char) * strlen("/dev/null") +1);
        strcpy(flags->outfile, "/dev/null");
    }

    return 0;
}

int
parseRedir_in(char *str, tflags *flags)
{
    char *token, *saveptr;
    
    if (flags->fin == 1) {
        token = strtok_r(str, "<", &saveptr); 
        if (token == NULL) {
            fprintf(stderr, "error: redirection in wrong place\n");
            wstatus = -1;
            return -1;
        }
        if (clear_spaces(saveptr) < 0) {
            wstatus = -1;
            return -1;
        }
        flags->infile = malloc(sizeof(char) * strlen(saveptr) +1);  
        strcpy(flags->infile, saveptr);
        str = token; 
    }

    return 0;
}

int
parseRedir(char *str, tflags *flags)
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
parseBg(char *str, tflags *flags)
{
    char *token, *saveptr;
    
    if (flags->fbg == 1) {
        token = strtok_r(str, "&", &saveptr); 
        if (token == NULL) {
            fprintf(stderr, "error: background in wrong place\n");
            wstatus = -1;
            return -1;
        }
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
    char *value;
    
    if (clear_spaces(str) < 0) {
        return -1;
    }
    str = str + 1;
    value = getenv(str);
    if (value == NULL) {
        fprintf(stderr, "error: no coincidences with :$%s\n", str);
        wstatus = -1;
        return -1;
    }
    instructions->ex[num][n] = malloc(sizeof(char) * strlen(value) + 1);
    strcpy(instructions->ex[num][n],value);    

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

    if (*command[0] == '.') {
        return 0;
    } else {
     
        for (i = 0; i < paths.num; i++) {
            // Asignar memoria para command_path
            command_path = malloc(strlen(paths.order[i]) + strlen(*command) + 1);
            if (command_path == NULL) {
                // Manejar error de asignación de memoria
                return -1;
            }
            strcpy(command_path, paths.order[i]);
            strcat(command_path, *command);
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
    }
    fprintf(stderr, "error: command not found\n");
    return -1;
}

int
firstpipe(int *fd, char **ex, int in_file, int out_file, int n)
{
	int pid;
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
        } else if (n == 1 && out_file == -1){
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
lastpipe(int *fd, char **ex, int out_file)
{
	int pid;

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
        wstatus = -1;
    }

    if (setenv(name, value, 1) < 0) {
        return -1;
        wstatus = -1;
    }

    return 0;
}

int handleRedir(char *file, int fflags, int mode) {
    int fd;
    
    fd = open(file, fflags, mode);
    if (fd < 0) {
        fprintf(stderr, "error: cannot open file\n");
        exit(EXIT_FAILURE);
    }
    return fd;
}

int main(int argc, char *argv[]) {

    if (argc != 1) {
        exit(EXIT_FAILURE);
    }

    char input[BUFFERSIZE];
    tpath paths;
    tcommand instructions;
    tflags flags;
    int i, j, in_file, out_file;
    int fd[2], prev_fd[2];
    wstatus = 0;

    clear();
   
    while (wstatus == 0) {
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
        input[strlen(input) -1] = '\0';
        in_file = -1;
        out_file = -1;
        // Check for exit command
        if (strcmp(input, "exit") == 0) {
            for (i = 0; i < paths.num; i++) {
                free(paths.order[i]);
            }
            free(paths.order);
            exit(EXIT_SUCCESS);
        }

        if (strcmp(input, "\0") != 0) {
            if (searchFlags(input, &flags) == -1) {
                exit(EXIT_FAILURE);
            }
            if (parseRedir(input, &flags) == -1) {
                exit(EXIT_FAILURE);
            }
            if (parseBg(input, &flags) == -1) {
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
                //printf("Numero de comandos: %d\n", instructions.ncommands);
                
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
                    //printf("Comando sin parsear: %s\n", instructions.array[i]);
                    if (parseSpace(&instructions, i) == -1) {
                        exit(EXIT_FAILURE);
                    }
                    if (strcmp(instructions.ex[0][0], "cd") == 0) {
                        break;
                    }
                    if (find_command_path(&instructions.ex[i][0], paths) == -1) {
                        exit(EXIT_FAILURE);
                    }
                    /*printf("Comando parseado %d con %d extras es: ", i, instructions.extras[i]);
                    for (j = 0; j < instructions.extras[i]; j++) {
                        printf("%s\n", instructions.ex[i][j]);
                    }*/
                }
                if (strcmp(instructions.ex[0][0], "cd") == 0) {
                    if (instructions.extras[0] > 2) {
                        fprintf(stderr, "error: cd [dir]\n");
                    } else if (instructions.extras[0] == 1) {
                        change_directory(NULL);
                    } else { 
                        change_directory(instructions.ex[0][1]);
                    }
                } else {
                    //printf("Redirecciones: IN: %s, OUT: %s\n", flags.infile, flags.outfile);
                    /* Ejecutar el primer comando */
                    if (flags.fin == 1) {
                        in_file = handleRedir(flags.infile, O_RDONLY, S_IRUSR);
                    }
                    if (flags.fout == 1) {
                        out_file = handleRedir(flags.outfile, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
                    } else if (flags.fbg == 1 ) {
                        out_file = handleRedir(flags.outfile, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
                    }
                    if (firstpipe(fd, instructions.ex[0], in_file, out_file, instructions.ncommands) == -1) {
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
                    if (instructions.ncommands > 1) {
                        if (lastpipe(prev_fd, instructions.ex[instructions.ncommands-1], out_file) == -1) {
                            exit(EXIT_FAILURE);
                        }
                    }
                    /* Esperar a que terminen todos los procesos */
                    while (wait(NULL) > 0);
                }
                /*Liberar memoria*/
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
            if (flags.fin == 1) {
                close(in_file);
                free(flags.infile);
            }
            if (flags.fout == 1 || flags.fbg == 1) {
                close(out_file);
                free(flags.outfile);
            }
        }
        
        for (i = 0; i < paths.num; i++) {
            free(paths.order[i]);
        }
        free(paths.order);
    }
	exit(0);
}