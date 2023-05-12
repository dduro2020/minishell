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