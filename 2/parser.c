#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

cmd *parseCommand(char *str)
{
    cmd *command = (cmd *)malloc(sizeof(cmd));

    char *cur = str;

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

        {
            command->name = malloc(len + 1);
            strncpy(command->name, str+start, len);
            command->name[len] = 0;

            command->args = malloc(strlen(str) - (cur - str) - strspn(cur, " ") + 1);
            strncpy(command->args, str+(cur - str) + strspn(cur, " "), strlen(str) - (cur - str) - strspn(cur, " "));
            command->args[strlen(str) - (cur - str) - strspn(cur, " ")] = 0;

            command->full_cmd = malloc(strlen(str) + 1);
            strncpy(command->full_cmd, str, strlen(str));
            command->full_cmd[strlen(str)] = 0;
            // printf("%ld, %d, %ld\n", strlen(str), len, strlen(str) - (cur - str) - strspn(cur, " "));
        }
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
        else if (i + 1 < len && ((str[i] == '>' && str[i + 1] == '>') || (str[i] == '&' && str[i + 1] == '&') || (str[i] == '|' && str[i + 1] == '|')))
        {
            ++ans;
            ++i;
        }
        else if (str[i] == '>' || str[i] == '|')
        {
            ++ans;
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
        else if (i + 1 < len && ((str[i] == '>' && str[i + 1] == '>') || (str[i] == '&' && str[i + 1] == '&') || (str[i] == '|' && str[i + 1] == '|')))
        {
            char *tmp = malloc(i + 1 - start);
            strncpy(tmp, &str[start], i - start);
            tmp[i - start] = 0;
            cmds[cur] = parseCommand(tmp);
            free(tmp);
            cmds[cur]->pipe = (str[i] == '>' ? DARROW : (str[i] == '&' ? AND : OR));
            cur++;

            ++i;
            start = i + 1;
            // printf("!PIPE >> at %d!\n", i);
        }
        else if (str[i] == '>' || str[i] == '|')
        {
            char *tmp = malloc(i + 1 - start);
            strncpy(tmp, &str[start], i - start);
            tmp[i - start] = 0;
            cmds[cur] = parseCommand(tmp);
            free(tmp);
            cmds[cur]->pipe = str[i] == '>' ? ARROW : PIPE;
            cur++;

            start = i + 1;
            // printf("!PIPE > or | at %d!\n", i);
        }
        else if (str[i] == '#')
        {
            // printf("!# at %d!\n", i);
            break;
        }
    }
    char *tmp = malloc(i + 1 - start);
    strncpy(tmp, &str[start], i - start);
    tmp[i - start] = 0;
    // printf("!!!%s", tmp);
    cmds[cur] = parseCommand(tmp);
    cmds[cur]->pipe = NOPIPE;
    free(tmp);
    return cmds;
}

char inputString(FILE *fp, char** str, size_t size)
{
    // The size is extended by the input with the value of the provisional
    int ch;
    int slash = 0;
    char quotes = '\0';
    size_t len = 0;
    *str = realloc(NULL, sizeof(**str) * size); // size is start size
    // if (!(*str))
    //     return 1;
    while (EOF != (ch = fgetc(fp)))
    {
        if (slash)
            slash = 0;
        else
        {
            if (ch == '\\')
            {
                slash = 1;
            }
            else if (quotes)
            {
                if (ch == quotes)
                {
                    quotes = '\0';
                }
            }
            else
            {
                if (ch == '\"' || ch == '\'')
                {
                    quotes = ch;
                }
                else
                {
                    if (ch == '\n')
                        break;
                }
            }
        }
        (*str)[len++] = ch;
        if (len == size)
        {
            *str = realloc(*str, sizeof(**str) * (size *= 2));
            if (!(*str))
                return 1;
        }
    }
    (*str)[len++] = '\0';
    *str = realloc(*str, sizeof(**str) * len);
    return ch;
}
