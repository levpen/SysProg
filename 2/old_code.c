//parser.h

#pragma once

typedef struct cmd
{
    char *name;
    char *args;
    char *full_cmd;
    char **argv;
    int argc;
    int pipe;
} cmd;

void printCmd(cmd *command);

void freeCmd(cmd *command);

//command.c

#include "command.h"
#include <stdio.h>
#include <stdlib.h>

void printCmd(cmd *command)
{
    printf("Command name: %s\n", command->name);
    printf("Command args: %s\n", command->args);
    printf("Command full: %s\n", command->full_cmd);
    printf("argc: %d\n", command->argc);
    for (int i = 0; i < command->argc; ++i)
    {
        printf("argv %d: %s\n", i, command->argv[i]);
    }
    printf("pipe: %d\n", command->pipe);
}

void freeCmd(cmd *command)
{
    free(command->name);
    free(command->args);
    free(command->full_cmd);
    free(command->argv);
    free(command);
}

//parser.h

#pragma once

#include "command.h"

#define NOPIPE 0
#define PIPE 1
#define ARROW 2
#define DARROW 3

int countArgs(char *str);

cmd *parseCommand(char *str);

int countCommands(char *str);

cmd **parseString(char *str, int n);

//parser.c

#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int countArgs(char *str)
{
    char *cur = str;
    int ans = 0;
    while (*cur)
    {
        cur += strspn(cur, " ");
        if (*cur == '\"')
        {
            ++cur;
            cur += strcspn(cur, "\"");
            while (*(cur - 1) == '\\')
            {
                ++cur;
                cur += strcspn(cur, "\"");
            }
            ++cur;
        }
        else if (*cur == '\'')
        {
            ++cur;
            cur += strcspn(cur, "\'");
            ++cur;
        }
        else
        {
            cur += strcspn(cur, " ");
        }
        cur += strspn(cur, " ");
        ++ans;
    }
    return --ans;
}

cmd *parseCommand(char *str)
{
    cmd *command = (cmd *)malloc(sizeof(cmd));
    int args = countArgs(str);
    //printf("%s countArgs: %d\n", str, args);

    char *cur = str;

    command->argv = malloc(args * sizeof(char *));
    command->argc = 0;

    int first_time = 1;

    while (*cur)
    {
        cur += strspn(cur, " ");
        int start = cur - str;
        if (*cur == '\"')
        {
            ++cur;
            cur += strcspn(cur, "\"");
            while (*(cur - 1) == '\\')
            {
                ++cur;
                cur += strcspn(cur, "\"");
            }
            ++cur;
        }
        else if (*cur == '\'')
        {
            ++cur;
            cur += strcspn(cur, "\'");
            ++cur;
        }
        else
        {
            cur += strcspn(cur, " ");
        }

        int len = cur - str - start;
        if (first_time)
        {
            first_time = 0;
            command->name = malloc(len + 1);
            strncpy(command->name, &str[start], len);
            command->name[len] = 0;

            command->args = malloc(strlen(str)-len+1-strspn(cur, " "));
            strncpy(command->args, &str[len+strspn(cur, " ")], strlen(str)-len+1-strspn(cur, " "));

            command->full_cmd = malloc(strlen(str)+1);
            strncpy(command->full_cmd, str, strlen(str)+1);
        }
        else
        {
            command->argv[command->argc] = malloc(len + 1);
            strncpy(command->argv[command->argc], &str[start], len);
            command->argv[command->argc++][len] = 0;
        }
        cur += strspn(cur, " ");
    }

    return command;
}

int countCommands(char *str)
{
    int len = strlen(str);
    int ans = 0;

    for (int i = 0; i < len; ++i)
    {
        if (str[i] == '\"')
        {
            while (++i < len)
            {
                if (str[i] == '\"' && str[i - 1] != '\\')
                    break;
            }
        }
        else if (str[i] == '\'')
        {
            while (++i < len && str[i] != '\'')
            {
            }
        }
        else if (i + 1 < len && str[i] == '>' && str[i + 1] == '>')
        {
            ++ans;
            ++i;
            // printf("!COMMAND PIPE >> AT %d\n", i);
        }
        else if (str[i] == '>' || str[i] == '|')
        {
            ++ans;
            // printf("!COMMAND PIPE AT %d\n", i);
        }
    }
    return ans + 1;
}

cmd **parseString(char *str, int n)
{
    int len = strlen(str);

    cmd **cmds = malloc(n * sizeof(cmd *));
    int cur = 0;
    int start = 0;
    int i = 0;
    for (; i < len; ++i)
    {
        if (str[i] == '\"')
        {
            while (++i < len)
            {
                if (str[i] == '\"' && str[i - 1] != '\\')
                    break;
            }
        }
        else if (str[i] == '\'')
        {
            while (++i < len && str[i] != '\'')
            {
            }
        }
        else if (i + 1 < len && str[i] == '>' && str[i + 1] == '>')
        {
            cmds[cur] = malloc(sizeof(cmd));
            char *tmp = malloc(i + 1 - start);
            strncpy(tmp, &str[start], i - start);
            tmp[i - start] = 0;
            cmds[cur] = parseCommand(tmp);
            free(tmp);
            cmds[cur]->pipe = DARROW;
            cur++;

            ++i;
            start = i + 1;
            printf("!PIPE >> at %d!\n", i);
        }
        else if (str[i] == '>' || str[i] == '|')
        {
            cmds[cur] = malloc(sizeof(cmd));
            char *tmp = malloc(i + 1 - start);
            strncpy(tmp, &str[start], i - start);
            tmp[i - start] = 0;
            cmds[cur] = parseCommand(tmp);
            free(tmp);
            cmds[cur]->pipe = str[i] == '>' ? ARROW : PIPE;
            cur++;

            start = i + 1;
            printf("!PIPE > or | at %d!\n", i);
        }
        else if(str[i] == '#') {
            printf("!# at %d!\n", i);
            break;
        }
    }
    cmds[cur] = malloc(sizeof(cmd));
    char *tmp = malloc(i + 1 - start);
    strncpy(tmp, &str[start], i - start);
    tmp[i - start] = 0;
    // printf("!!!%s\n", tmp);
    cmds[cur] = parseCommand(tmp);
    cmds[cur]->pipe = NOPIPE;
    free(tmp);

    return cmds;
}

//