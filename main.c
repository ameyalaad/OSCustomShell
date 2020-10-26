#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#define TOKEN_BUFFER_SIZE 64
#define TOK_PIPE_DELIM "|"
#define COMMAND_DELIMITERS " \t\r\n\a"
#define READ_END 0
#define WRITE_END 1
#define COMMAND_LOG "command.log"
#define OUTPUT_LOG "output.log"
// Builtin commands
int bi_entry(int* entered, int* exited){
    *entered = 1;
    *exited = 0;
    return 1;
}
int bi_exit(int* entered, int* exited){
    *entered = 0;
    *exited = 1;
    return 1;
}
int bi_log(int* logging){
    *logging = 1;
    return 1;
}
int bi_unlog(int* logging){
    *logging = 0;
    return 1;
}
int bi_viewcmdlog(){
    return 1;
}
int bi_viewoutlog(){
    return 1;
}
int bi_changedir(char** sepcommand){
    return 1;
}


int launch_command(char** command, int** pipes, int comindex, int num_commands){ 
    pid_t pid, wpid;
    int status;
    // write(STDOUT_FILENO, "Executing process\n", 19*sizeof(char));
    pid = fork();
    if(pid == 0){
        // check if ONLY ONE PROCESS (num_commands==0 should never occur)
        if(num_commands==1){
            // Don't change anything
        }
        // setup checks for the first and last process ONLY
        else if(comindex==0){
            // write(STDOUT_FILENO, "In first command\n", 18*sizeof(char));
            // stdin remains same, stdout for child to pipe 
            if(dup2(pipes[comindex][WRITE_END], STDOUT_FILENO)==-1){
                perror("dup2: first command");
                exit(1);
            }
        } else if(comindex==num_commands-1){
            // write(STDOUT_FILENO, "In last command\n", 17*sizeof(char));
            // stdout remains same, stdin will be previous pipe
            close(pipes[comindex-1][WRITE_END]);
            if(dup2(pipes[comindex-1][READ_END], STDIN_FILENO)==-1){
                perror("dup2: last command");
                exit(1);
            }
        } else {
            // duplicate both file descriptors
            close(pipes[comindex-1][WRITE_END]);
            if(dup2(pipes[comindex-1][READ_END], STDIN_FILENO)==-1){
                perror("dup2: inter command: READ_END");
                exit(1);
            }
            if(dup2(pipes[comindex][WRITE_END], STDOUT_FILENO)==-1){
                perror("dup2: inter command: WRITE_END");
                exit(1);
            }
        }
        // Run command
        if(execvp(command[0], command) == -1){
            // TODO: We also need to do a check inside the cwd
            perror("execvp");
        }
        exit(1);
    } else if (pid < 0){
        // error forking
        perror("fork");
        return 0;
    } else {
        // parent process
        do {
            // close pipe fd to succesfully execute
            if(num_commands!=1 && comindex!=num_commands-1){
                // close current pipe
                // close(pipes[comindex][READ_END]); // DO NOT CLOSE READ END AS NEXT PROCESS WILL READ FROM IT
                close(pipes[comindex][WRITE_END]); 
            } 
            wpid = waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int execute_commands(char*** commands, int num_commands, int* entered, int* exited, int* logging){
    // printf("Executing with %d commands\n", num_commands);
    // Creating a 2-D (pointer) array of 2*(n-1) integers, for pipe file descriptors
    int** pipes = malloc((num_commands-1)* sizeof(int*));
    // pipes -> pipe1 -> READ_END
    //                -> WRITE_END
    //       -> pipe2
    //       -> pipe3   
    // Initialize pipes
    for (int i = 0; i < num_commands-1; i++)
    {
        int* tpipe= malloc(2*sizeof(int));
        if(pipe(tpipe)==-1){
            perror("pipe");
            exit(1);
        }
        pipes[i]=tpipe;       
    }
    

    // start execution until NULL in commands (can now be looped using the num_commands as well)
    int comindex=0;
    char** command = commands[comindex];
    int execstatus;

    do{
        if(command == NULL){
            // Last command reached, prompt for new commands
            return 1;
        }
        if(strcmp(command[0], "entry")==0){
            execstatus = bi_entry(entered, exited);
        } 
        else if(*exited==1){
            printf("Command line interpreter exited\n");
            return 1;
        }else if (*entered==0) {
            printf("Command line interpreter not started\n");
            return 1;
        }else{
            //check for other builtins as well
            if(strcmp(command[0], "exit")==0){
                execstatus = bi_exit(entered, exited);
            } else if (strcmp(command[0], "log")==0){
                execstatus = bi_log(logging);
            } else if (strcmp(command[0], "unlog")==0){
                execstatus = bi_unlog(logging);
            } else if (strcmp(command[0], "viewcmdlog")==0){
                execstatus = bi_viewcmdlog();
            } else if (strcmp(command[0], "viewoutlog")==0){
                execstatus = bi_viewoutlog();
            } else if (strcmp(command[0], "changedir")==0){
                execstatus = bi_changedir(command);
            } else {
                // Not an internal command, run exec
                execstatus = launch_command(command, pipes, comindex, num_commands);
            }
        }
        comindex++;
        command = commands[comindex];
    } while(execstatus);

    

    // not an internal/builtin command (should never be reached?)
    printf("Error reurned from launch_command. Debug for further details");
    return 0;
}

char*** parse_command_args(char** nsepcommands){
    int commandbufsize = TOKEN_BUFFER_SIZE, commandnumber=0;
    char*** commands = malloc(commandbufsize * sizeof(char**));
    char* nsepcommand = nsepcommands[0]; //command line to now seperate
    if(!commands){
        fprintf(stderr, "ash: malloc error while seperating commands\n");
        exit(1);
    }
    while(nsepcommand!=NULL){ //loop over commands
        // printf("Command is: %s\n", nsepcommand);
        //seperate the command
        int bufsize = TOKEN_BUFFER_SIZE, argsposition=0;
        char** sepcommand = malloc(bufsize * sizeof(char*));
        char* args;

        if(!sepcommand){
            fprintf(stderr, "ash: malloc error while seperating commands\n");
            exit(1);
        }

        args = strtok(nsepcommand, COMMAND_DELIMITERS);

        while(args!=NULL){ //save in commands array

            // Debug print statement
            // printf("args#%d: %s \n", argsposition, args);
            sepcommand[argsposition] = args;
            argsposition++;

            if(argsposition>=bufsize){
                // printf("Reallocating memory for single command\n");
                bufsize+=TOKEN_BUFFER_SIZE;
                sepcommand = realloc(sepcommand, bufsize * sizeof(char*));
                if(!sepcommand){
                    fprintf(stderr, "ash: malloc error while seperating commands\n");
                    exit(1);
                }
            }

            args = strtok(NULL, COMMAND_DELIMITERS);
        }
        sepcommand[argsposition] = NULL;
        commands[commandnumber] = sepcommand;
        commandnumber++;
        nsepcommand = nsepcommands[commandnumber];

        if(commandnumber>=commandbufsize){
            // printf("Reallocating memory for all commands array\n");
            commandbufsize+=TOKEN_BUFFER_SIZE;
            commands = realloc(commands, commandbufsize * sizeof(char*));
            if(!commands){
                fprintf(stderr, "ash: malloc error while seperating commands\n");
                exit(1);
            }
        }
    }
    commands[commandnumber]=NULL;
    return commands;
}

char** parse_line_to_nsep_commands(char* line, int* num_commands){
    int bufsize = TOKEN_BUFFER_SIZE, position=0;
    char** nsepcommands = malloc(bufsize * sizeof(char*));
    char* nsepcommand;

    if(!nsepcommands){
        fprintf(stderr, "ash: malloc error while parsing commands\n");
        exit(1);
    }

    nsepcommand = strtok(line, TOK_PIPE_DELIM);

    while(nsepcommand!=NULL){
        nsepcommands[position] = nsepcommand;
        position++;
        if(position>=bufsize){
            bufsize+=TOKEN_BUFFER_SIZE;
            nsepcommands = realloc(nsepcommands, bufsize * sizeof(char*));
            if(!nsepcommands){
                fprintf(stderr, "ash: malloc error while parsing commands\n");
                exit(1);
            }
        }

        nsepcommand = strtok(NULL, TOK_PIPE_DELIM);
    }
    *num_commands = position;
    nsepcommands[position]=NULL;
    return nsepcommands;
}

char* ash_readline(){
    char *line = NULL;
    size_t bufsize = 0; // have getline allocate a buffer for us

    if (getline(&line, &bufsize, stdin) == -1){
        if (feof(stdin)) {
            exit(0);  // We recieved an EOF
        } else  {
            perror("readline");
            exit(1);
        }
    }

    return line;
}

void commandloop(){
    // char cwd[PATH_MAX];
    int status = 1;
    char *line;
    char** nsepcommands;
    char ***commands; // 3-D array to store commands and args
    int num_commands = -1; //just setting an invalid default value
    int entered = 0;
    int exited = 0;
    int logging = 0;

    do{
        // if (getcwd(cwd, sizeof(cwd)) == NULL){
        //     perror("getcwd");
        // }
        printf("$ ");
        line = ash_readline();
        nsepcommands = parse_line_to_nsep_commands(line, &num_commands);
        commands = parse_command_args(nsepcommands);
        status = execute_commands(commands, num_commands, &entered, &exited, &logging); //external pipe? internal pipe
    }while(status);
}

int main(int argc, char const *argv[])
{
    commandloop();
    return 0;
}
