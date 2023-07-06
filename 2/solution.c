#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "parser.h"

int spawn_proc(int in, int out, cmd *command)
{
    if (strcmp(command->name, "exit") == 0 && command->pipe == NOPIPE && in == 0)
    {
        int err = atoi(command->args);
        if (!err)
            err = -1;
        // printf("%d\n", err);
        return err;
    }
    if (strcmp(command->name, "cd") == 0 && command->pipe == NOPIPE && in == 0)
    {
        chdir(command->args);
        return 0;
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

        return execl("/bin/bash", "bash", "-c", command->full_cmd, NULL);
    }
    return 0;
}

int execCmds(cmd **cmds, int n)
{
    int in, fd[2];
    in = 0;
    int error_code = 0;
    for (int i = 0; i < n; ++i)
    {
        int file = 0;
        if (cmds[i]->pipe == AND || cmds[i]->pipe == OR || i == n - 1)
        {
            int status;
            error_code = spawn_proc(in, 1, cmds[i]);
            while (wait(&status) > 0)
                ;
            if (cmds[i]->pipe == OR && status == 0)
            {
                while (cmds[i]->pipe == OR)
                    ++i;
            }
            else if (cmds[i]->pipe == AND && status)
            {
                return 0;
            }
        }
        else
        {
            pipe(fd);
            if (cmds[i]->pipe == ARROW || cmds[i]->pipe == DARROW)
            {
                char *file_desc = malloc(strlen(cmds[i + 1]->name) + 1);
                if (cmds[i + 1]->name[0] == '\"')
                {
                    strncpy(file_desc, cmds[i + 1]->name + 1, strlen(cmds[i + 1]->name) - 2);
                    file_desc[strlen(cmds[i + 1]->name) - 2] = 0;
                }
                else
                {
                    strncpy(file_desc, cmds[i + 1]->name, strlen(cmds[i + 1]->name));
                    file_desc[strlen(cmds[i + 1]->name)] = 0;
                }
                if (cmds[i]->pipe == ARROW)
                    file = creat(file_desc, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                else if (cmds[i]->pipe == DARROW)
                    file = open(file_desc, O_CREAT | O_WRONLY | O_APPEND);
                // printf("FD: %d. %s\n", file, file_desc);
                error_code = spawn_proc(in, file, cmds[i++]);
                free(file_desc);
            }
            else
            {
                error_code = spawn_proc(in, fd[1], cmds[i]);
            }
            close(fd[1]);
            in = fd[0];
        }

        // wait(NULL);
        // printf("CHILD_PID: %d\n", child_pid);
        // printf("STATUS: %d\n", status);

        if (file)
        {
            close(file);
        }

        if (error_code)
        {
            return error_code;
        }
    }
    return 0;
}

int main()
{
    char *str = '\0';
    // FILE *f = fopen("test.txt", "r");
    while (1)
    {
        // printf("$> ");
        char rez = inputString(stdin, &str, 10);
        int n = countCommands(str);

        // printf("%s\n", str);
        // printf("%d\n", rez);
        // printf("%d\n", n);
        cmd **cmds = parseString(str, n);
        int err = 0;
        err = execCmds(cmds, n);
        while (wait(NULL) > 0)
            ;
        for (int i = 0; i < n; ++i)
        {
            // printCmd(cmds[i]);
            freeCmd(cmds[i]);
        }
        free(cmds);
        free(str);
        // break;
        if (err || rez == -1)
        {
            if (err == -1)
                err = 0;
            exit(err);
        }
    }

    return 0;
}