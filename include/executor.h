
/* execute with pipes */
int firstpipe(int *fd, char **ex, int in_file, int out_file, int n);
int pipes(int *fd1, int *fd2, char **ex);
int lastpipe(int *fd, char **ex, int out_file);

/* define envars */
int defvars(char *inputString);

/* open required files */
int handleRedir(char *file, int fflags, int mode);