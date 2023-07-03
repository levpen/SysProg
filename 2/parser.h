#pragma once

#include "command.h"

#define NOPIPE 0
#define PIPE 1
#define ARROW 2
#define DARROW 3

cmd *parseCommand(char *str);

int countCommands(char *str);

cmd **parseString(char *str, int n);




