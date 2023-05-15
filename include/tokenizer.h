
/* clear shell */
void clear();

/* delete all blanc spaces in words */
int clear_spaces(char *str);

/* checks if input are blanc spaces */
int check_input(char *str);

/* check every falg in command line */
int searchFlags(char *str, tflags *flags);

/* modify in/out files */
int parseRedir_out(char *str, tflags *flags);
int parseRedir_in(char *str, tflags *flags);
int parseRedir(char *str, tflags *flags);

/* tokenize & */
int parseBg(char *str, tflags *flags);

/* count $PATH paths */
int countDots(char *PATH);

/* count n commands separed by | */
int countPipes(char *str);

/* save all executable paths */
int splitPath(char *PATH, tpath *paths);

/* tokenize by | */
int parsePipe(char *inputString, tcommand *instructions);

/* count n words in each command (after parsePipe) */
int countWords(char *str);

/* replace $env by their value */
int replace_env_vars(tcommand *instructions, int num, char *str, int n);

/* tokenize words after countWords */
int parseSpace(tcommand *instructions, int num);

/* look for executable path of each command */
int find_command_path(char **command, tpath paths);
