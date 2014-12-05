#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#define INTER_MODE 0
#define BATCH_MODE 1
#define FORK_ERR -1
#define BUFF_LEN 1024

extern int errno;

/* Utility function for trimming white space from a command */
char* trimspace(char *str)
{
    char *end;

    // trim leading space
    while (isspace(*str)) str++;

    // all spaces
    if (*str == 0)
        return str;

    // trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    *(end+1) = '\0';

    return str;
}

/* Command to handle executing commands from forked process (execvp) */
void execcmd(char *cmd, int out, int mode)
{
    int eStat = EXIT_SUCCESS;
    char *args[4];
    args[0] = "sh";
    args[1] = "-c";
    args[3] = NULL;
    
    // if command is quit, write it to pipe
    if (!memcmp(cmd, "quit", 4)) {
        int nbytes = write(out, "quit", strlen("quit"));
        
        if (nbytes != strlen("quit")) {
            fprintf(stderr, "Error: Did not write properly, %d\n", nbytes);
            fprintf(stderr, "%s\n", strerror(errno));
            eStat = EXIT_FAILURE;
        }
        
        close(out);
        exit(eStat);
    }
    // else execute command
    else {
        close(out);
        args[2] = cmd;
        execvp(*args, args);
        exit(EXIT_FAILURE);
    }
}

/* Processes a line of commands and spawns a child process to execute
 * a command */
void proccmd(char *line, int *fd, int mode)
{
    char *tok;
    pid_t pid;
    int status;
   
    // strip newline if it exists
    size_t nl = strlen(line) - 1;
    if (line[nl] == '\n')
        line[nl] = '\0';

    if (mode == BATCH_MODE)
        printf(">> %s\n", line);

    tok = strtok(line, ";");
    while (tok) {
        // remove leading and trailing whitespace
        tok = trimspace(tok);
        
        // execute commands
        if ((pid = fork()) < 0) {   // error
            fprintf(stderr, "Error: Failed to fork\n%s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {    // child
            // close output
            close(fd[0]);
            execcmd(tok, fd[1], mode);
            // exec returned
            fprintf(stderr, "Error: Exec returned\n%s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else {    // parent
            // nothing to do
        }

        tok = strtok(NULL, ";");
    }
}

int pollreader(int in)
{
    char buff[1024];
    int eStat = EXIT_FAILURE;

    // check if quit was written
    if (poll(&(struct pollfd){ .fd=in, .events=POLLIN }, 1, 0)==1) {
        // try to read
        int nbytes = read(in, buff, sizeof buff);
        if (nbytes < 0) {
            fprintf(stderr, "Error: Could not read from pipe\n");
            fprintf(stderr, "%s\n", strerror(errno));
            close(in);
            eStat = EXIT_FAILURE;
        }
        else if (!memcmp(buff, "quit", 4))
            eStat = 4;
    }
    else
        eStat = EXIT_SUCCESS;

    return eStat;
}

int main(int argc, char *argv[])
{
    char line[BUFF_LEN];
    int quit = 0;

    if (argc > 2) {
        fprintf(stderr, "Error: Unexpected arg count\n");
        fprintf(stderr, "Usage: <shell> [batchFile]\n");
        exit(EXIT_FAILURE);
    }

    int fd[2];
    if (pipe(fd)) {
        fprintf(stderr, "Error: Pipe failed\n");
        exit(EXIT_FAILURE);
    }

    // interactive mode
    if (argc == 1) {
        while (1) {
            printf(">> ");

            // get a line of input from terminal
            if (fgets(line, sizeof line, stdin) != NULL) {
                // process a line of commands
                proccmd(line, fd, INTER_MODE);
                
                // wait for all commands to finish
                while (wait(NULL) > 0)
                    ;
                
                // poll read fd for quit command
                int stat = pollreader(fd[0]);
                if (stat == EXIT_FAILURE)
                    exit(stat);
                else if (stat == strlen("quit"))
                {
                    printf("\nExiting...\n");
                    break;
                }
            }
            else
                printf("Dafuq, how did you make that null\n");
        } 
    }
    // batchfile mode
    else {
        FILE *batch = fopen(argv[1], "r");
        char bytes[1024];

        if (batch == NULL) {
            fprintf(stderr, "Error: Unable to open file\n%s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // read each line of input file
        while (fgets(line, sizeof line, batch) != NULL) {  
            proccmd(line, fd, BATCH_MODE);
            
            while (wait(NULL) > 0)
                ;

            int stat = pollreader(fd[0]);
            if (stat == EXIT_FAILURE)
                exit(stat);
            else if (stat == strlen("quit"))
            {
                printf("\nExiting...\n");
                break;
            }
        }

        fclose(batch);
    }

    return 0;
}
