#pragma once

#include "command.h"
#include <stdio.h>

#define NOPIPE 0
#define PIPE 1
#define ARROW 2
#define DARROW 3
#define AND 4
#define OR 5

cmd *parseCommand(char *str);

int countCommands(char *str);

cmd **parseString(char *str, int n);

char inputString(FILE* fp, char **str, size_t size);




