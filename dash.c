//
//  dash.c
//  dash
//  version 1.3
//
//  Created by Haoda LE on 2019/9/11.
//  Copyright © 2019 haoda le. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include<fcntl.h>


//global variable, the initial system command searchning path
char sysCmdPath[200]="/bin /usr/bin";
//global variable, all build in function we have.
char *buildinCmd[]={"exit","help","cd","path"};

int main(int argc, const char * argv[]) {
    void ExecCommand(char **cmds,char *filename,bool redir);
    char **SplitCommand(char *line,char *delim);
    void ErrorMessage(char *errMsg);
    void CommandProcess(char *line);
    
    //batch mode, get filename from argv[1]
    if(argv[1]!=NULL){
        //if the shell is invoke more than one file.
        if(argv[2]!=NULL){
            ErrorMessage("Invoke more than one file.");
            exit(1);
        }
        char const* const filename=argv[1];
        FILE *file=fopen(filename, "r");
        char line[256];
        
        if(file==NULL){
            ErrorMessage("read file error!");
            exit(1);
        }
        
        while(fgets(line, sizeof(line), file)){
            //process command in batch file, line by line.
            CommandProcess(line);
        }
        
        fclose(file);
        exit(0);
    }
    
    
    //interactive mode, loop until exit command.
    while(1){
        printf("dash> ");
        
        char *line = NULL;
        size_t len=0;
        getline(&line,&len,stdin);
        
        //process input command.
        CommandProcess(line);
        
    }
    
    return 0;
}


//split the command by delim.
char **SplitCommand(char *line,char *delim){
    int bufSize=100;
    
    char **cmds=malloc(bufSize*sizeof(char *));
    char *cmd=NULL;
    
    int i=0;
    cmd=strtok(line,delim);
    
    while(cmd!=NULL){
        cmds[i]=cmd;
        i++;
        cmd=strtok(NULL,delim);
    }
    cmds[i]=NULL;
    return cmds;
}


//show error message.
void ErrorMessage(char *errMsg){
    if(strcmp(errMsg, "")!=0){
        perror(errMsg);
    }
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}


//execute the build in command and system command.
void ExecCommand(char **cmds,char *filename,bool redir){
    //build in commands numbers.
    int cmdLen=sizeof(buildinCmd)/sizeof(buildinCmd[0]);
    //match build in commands.
    int buildinCmdCode=-1;
    int i;
    for(i=0;i<cmdLen;i++){
        if(strcasecmp(buildinCmd[i],cmds[0])==0){
            buildinCmdCode=i;
            break;
        }
    }
    
    //build in command execution.
    if(buildinCmdCode!=-1){
        switch (buildinCmdCode) {
            case 0:
                //build in command: exit.
                if(cmds[1]!=NULL){
                    ErrorMessage("exit cannot have any arguments!");
                }
                else{
                    exit(0);
                }
                break;
            case 1:
                //build in command: help.
                printf("%s","**** This is a help function ****\n");
                printf("%s","We provide below build in functions:\n");
                for(i=0;i<cmdLen;i++){
                    printf("%s ",buildinCmd[i]);
                }
                printf("\n");
                break;
            case 2:
                //build in command: cd
                if(cmds[1]==NULL){
                    ErrorMessage("CD cannot find any argument!");
                }
                else if(cmds[2]!=NULL){
                    ErrorMessage("CD command can only take one argument!");
                }
                else {
                    if(chdir(cmds[1])!=0){
                        ErrorMessage("dash error");
                    }
                }
                break;
            case 3:
                //build in command: path
                strcpy(sysCmdPath, "");
                int j=1;
                while(cmds[j]!=NULL){
                    strcat(sysCmdPath, cmds[j]);
                    strcat(sysCmdPath," ");
                    j++;
                }
                break;
            default:
                break;
        }
    }
    
    //system function execution.
    else{
        int status;
        pid_t pid = fork();

        // child process
        if (pid == 0) {
            //prepare the command  searching path.
            char **cmdPaths=SplitCommand(sysCmdPath, " \t\r\n\a");
            bool findPath=false;
            
            int i=0;
            char *cmdPath=NULL;
            while(cmdPaths[i]!=NULL){
                cmdPath=(char *)malloc(strlen(cmdPaths[i])+strlen(cmds[0])+1);
                strcpy(cmdPath, cmdPaths[i]);
                strcat(cmdPath, "/");
                strcat(cmdPath, cmds[0]);
                
                //check access and execv
                if(access(cmdPath, X_OK)==0){
                    findPath=true;
                    //if not redirection, then execute directly.
                    if(!redir){
                        if(execv(cmdPath, cmds)==-1){
                            ErrorMessage("dash error");
                            exit(EXIT_FAILURE);
                        }
                    }
                    break;
                }
                free(cmdPath);
                i++;
            }
            
            //if redirection, output result.
            if(redir){
                int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
                if(findPath){
                    execv(cmdPath,cmds);
                }
            }
            
            //error if cannot find command at any path.
            if(!findPath){
                ErrorMessage("Invalid Command! Please check your input or searching path!");
                exit(EXIT_FAILURE);
            }
        }
        
        // fork error
        else if (pid < 0) {
            ErrorMessage("dash fork error");
        }
        
        // parent process, wait child to finish.
        else {
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
}


//process the input/batch command first.
//check if it is parallel or redirection command. then ExecCommand
void CommandProcess(char *line){
    char **redCmds=NULL;
    char **parall=NULL;
    char **cmds=NULL;
    
    //Parallel check
    parall=SplitCommand(line, "&");
    
    int i=0;
    //if not parallel, run once. else run all parallel command.
    while(parall[i]!=NULL){
        cmds=NULL;
        //Redirection check
        //check redirection command error
        char *redErr=strstr(parall[i], ">>");
        if(redErr){
            printf("%s","Error: Multiple redirection operators!\n");
            ErrorMessage("");
            break;
        }
        //check redirection command error
        redCmds=SplitCommand(parall[i], ">");
        if(redCmds[1]!=NULL && redCmds[2]!=NULL){
            printf("%s","Error: Multiple redirection operators!\n");
            ErrorMessage("");
            break;
        }
        //REdirection
        else if(redCmds[1]!=NULL){
            char **filename=SplitCommand(redCmds[1], " \t\r\n\a");
            //check redirection command error.
            if(filename[1]!=NULL){
                printf("%s","Error: Multiple redirection files!\n");
                ErrorMessage("");
                break;
            }
            cmds=NULL;
            cmds=SplitCommand(redCmds[0]," \t\r\n\a");
            if(cmds[0]!=NULL){
                ExecCommand(cmds, filename[0], true);
            }
        }
        //NOT Redirection
        else{
            cmds=NULL;
            cmds=SplitCommand(redCmds[0]," \t\r\n\a");
            if(cmds[0]!=NULL){
                ExecCommand(cmds, NULL, false);
            }
        }
        i++;
    }
}



