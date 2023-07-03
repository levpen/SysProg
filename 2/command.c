#include "command.h"
#include <stdio.h>
#include <stdlib.h>

void printCmd(cmd *command)
{
    printf("Command name: %s\n", command->name);
    printf("Command args: %s\n", command->args);
    printf("Command full: %s\n", command->full_cmd);
    printf("pipe: %d\n", command->pipe);
}

void freeCmd(cmd *command)
{
    free(command->name);
    free(command->args);
    free(command->full_cmd);
    free(command);
}