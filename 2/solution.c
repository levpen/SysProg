#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "parser.h"

int spawn_proc(int in, int out, cmd *command)
{
    if(strcmp(command->name, "exit") == 0) {
        return -1;
    }
    pid_t pid;
    if ((pid = fork()) == 0)
    {
        if (in != 0)
        {
            dup2(in, 0);
            close(in);
        }

        if (out != 1)
        {
            dup2(out, 1);
            close(out);
        }
        if (strcmp(command->name, "cd") == 0)
        {
            chdir(command->args);
            return 0;
        }

        return execl("/bin/bash", "bash", "-c", command->full_cmd, NULL);
    }
    return pid;
}

void execCmds(cmd **cmds, int n)
{
    int in, fd[2];
    in = 0;
    int child_pid = 0;
    for (int i = 0; i < n; ++i)
    {
        int file = 0;
        if (i == n - 1)
        {
            child_pid = spawn_proc(in, 1, cmds[i]);
        }
        else
        {
            pipe(fd);
            if(cmds[i]-> pipe == ARROW || cmds[i]-> pipe == DARROW) {
                char *file_desc = malloc(strlen(cmds[i+1]->name)+1);
                if(cmds[i+1]->name[0] == '\"'){
                    strncpy(file_desc, cmds[i+1]->name+1, strlen(cmds[i+1]->name)-2);
                    file_desc[strlen(cmds[i+1]->name)-2] = 0;
                }
                else{
                    strncpy(file_desc, cmds[i+1]->name, strlen(cmds[i+1]->name));
                    file_desc[strlen(cmds[i+1]->name)] = 0;
                }
                if(cmds[i]-> pipe == ARROW)

                    file = creat(file_desc, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                else if(cmds[i]-> pipe == DARROW)
                    file = open(file_desc, O_CREAT | O_WRONLY | O_APPEND);
                printf("FD: %d. %s\n", file, file_desc);
                child_pid = spawn_proc(in, file, cmds[i++]);
                free(file_desc);
            } else {
                child_pid = spawn_proc(in, fd[1], cmds[i]);
            }
            close(fd[1]);
            in = fd[0];
        }
        wait(NULL);
        
        if(file) {
            close(file);
        }

        if(child_pid == -1) {
            for(int j = 0; j < n; ++j) {
                freeCmd(cmds[j]);
            }
            free(cmds);
            exit(0);
        }
    }
}

int main()
{
    char str[100000];
    printf("$ ");
    while (scanf("%[^\n]%*c", str))
    {
        while(str[strlen(str)-1] == '\\') {
            str[strlen(str)-1] = 0;
            scanf("%[^\n]%*c", &str[strlen(str)]);
        }
        int n = countCommands(str);
        // printf("%d\n", n);
        cmd **cmds = parseString(str, n);
        execCmds(cmds, n);
        for (int i = 0; i < n; ++i)
        {
            //printCmd(cmds[i]);
            freeCmd(cmds[i]);
        }
        free(cmds);
        //break;
        printf("$ ");
    }


    return 0;
}