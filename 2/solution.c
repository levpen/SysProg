#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "parser.h"

// void child_handler(int sig) {
//     printf("SIG %d\n", sig);
//     int status = 0;
//     waitpid(-1, &status, 0);
//     printf("STATUS: %d\n", status);
//     printf("WEXITSTATUS: %d\n", WEXITSTATUS(status));
//     // _exit(status);
// }

// returns not 0 if we should exit or something went wrong
int spawn_proc(int in, int out, cmd *command, int *exit_code, pid_t *child_pid)
{
    if (strcmp(command->name, "exit") == 0)
    {
        *exit_code = atoi(command->args);
    }
    if (strcmp(command->name, "exit") == 0 && command->pipe == NOPIPE && in == 0)
    {
        *exit_code = atoi(command->args);
        // printf("%d\n", exit_code);
        return 1;
    }
    if (strcmp(command->name, "cd") == 0 && command->pipe == NOPIPE && in == 0)
    {
        *exit_code = chdir(command->args);
        return 0;
    }

    if ((*child_pid = fork()) == 0)
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

        execl("/bin/bash", "bash", "-c", command->full_cmd, NULL);
    }
    return 0;
}

int execCmds(cmd **cmds, int n, int *exit_code)
{
    int in = 0, fd[2];
    int error_code = 0;
    pid_t *childs = malloc(sizeof(pid_t) * n);
    for (int i = 0; i < n; ++i)
    {
        int file = 0;
        if (cmds[i]->pipe == AND || cmds[i]->pipe == OR || i == n - 1)
        {
            int status;
            error_code = spawn_proc(in, 1, cmds[i], exit_code, &childs[i]);
            wait(&status);
            if (cmds[i]->pipe == OR && status == 0)
            {
                while (cmds[i]->pipe == OR)
                    ++i;
            }
            else if (cmds[i]->pipe == AND && status)
                return 0;
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
                error_code = spawn_proc(in, file, cmds[i], exit_code, &childs[i]);
                free(file_desc);
                ++i;
            }
            else
            {
                error_code = spawn_proc(in, fd[1], cmds[i], exit_code, &childs[i]);
            }
            close(fd[1]);
            in = fd[0];

            if (cmds[i]->pipe != PIPE)
            {
                int status;
                wait(&status);
                if (WIFEXITED(status))
                {
                    *exit_code = WEXITSTATUS(status);
                }
                else
                {
                    *exit_code = WTERMSIG(status);
                }
            }
        }

        if (file)
            close(file);

        if (error_code)
        {
            free(childs);
            return error_code;
        }
    }

    for (int i = 0; i < n; ++i)
    {
        int status = 0;
        waitpid(childs[i], &status, 0);
        
        // if(ans > 0) {
        //     if (WIFEXITED(status)) {
        //         *exit_code = WEXITSTATUS(status);
        //     } else {
        //         *exit_code = WTERMSIG(status);
        //     }
        // }
        // }

        // printf("STATUS: %d\n", status);
        // printf("PID: %d\n", childs[i]);
        // printf("ANS: %d\n", ans);
        // printf("WIFEXITED(status): %d\n", WIFEXITED(status));
        // printf("WEXITSTATUS: %d\n", WEXITSTATUS(status));
        // printf("WTERMSIG: %d\n", WTERMSIG(status));
        // printf("EXIT_CODE: %d\n", *exit_code);
    }
    free(childs);
    return 0;
}

int main()
{
    char *str = '\0';
    int exit_code = 0;
    // signal( SIGCHLD, SIG_IGN);

    while (1)
    {
        // printf("$> ");
        char rez = inputString(stdin, &str, 10);
        int n = countCommands(str);

        // printf("%s\n", str);
        // printf("%d\n", rez);
        // printf("%d\n", n);
        cmd **cmds = parseString(str, n);
        int err = execCmds(cmds, n, &exit_code);

        for (int i = 0; i < n; ++i)
        {
            // printCmd(cmds[i]);
            freeCmd(cmds[i]);
        }
        free(cmds);
        free(str);
        if (err || rez == -1)
        {
            break;
        }
    }

    return exit_code;
}