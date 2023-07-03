#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

cmd *parseCommand(char *str)
{
    cmd *command = (cmd *)malloc(sizeof(cmd));

    char *cur = str;

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
            break;
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
            // printf("!PIPE >> at %d!\n", i);
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
            // printf("!PIPE > or | at %d!\n", i);
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

char *inputString(FILE* fp, size_t size){
//The size is extended by the input with the value of the provisional
    char *str;
    int ch;
    char quotes = '\0';
    size_t len = 0;
    str = realloc(NULL, sizeof(*str)*size);//size is start size
    if(!str) return str;
    while(EOF!=(ch=fgetc(fp))){
        if(quotes) {
            if(ch == quotes && str[len-1] != '\\') {
                quotes = '\0';
            }
        } else {
            if(ch == '\"' || ch == '\'') {
                quotes = ch;
            } else {
                if(ch == '\n' && str[len-1] == '\\') {
                    --len;
                    continue;
                }
                if(ch == '\n')
                    break;
            }
        }
        str[len++]=ch;
        if(len==size){
            str = realloc(str, sizeof(*str)*(size*=2));
            if(!str)return str;
        }
    }
    str[len++]='\0';

    return realloc(str, sizeof(*str)*len);
}
