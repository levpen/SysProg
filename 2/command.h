#pragma once

typedef struct cmd
{
    char *name;
    char *args;
    char *full_cmd;
    int pipe;
} cmd;

void printCmd(cmd *command);

void freeCmd(cmd *command);
