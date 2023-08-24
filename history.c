
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "history.h"
#include "logger.h"

#define MIN(a,b) ((a<b)*a + (b<=a)*b)


struct hist_list{
    char** commands;
    unsigned int capacity;
    unsigned int size;
};

struct hist_list* create_hist_list(unsigned int limit)
{
    struct hist_list* hl = malloc(sizeof(struct hist_list));
    hl->commands = malloc(limit*sizeof(char*));
    hl->capacity = limit;
    hl->size = 0;
    return hl;
}

void destruct_hist_list(struct hist_list* hl)
{
    for (unsigned int i = 0; i < hl->size; i++) free(hl->commands[i]);
    free(hl);
}

struct hist_list* history;

void hist_init(unsigned int limit)
{
    history = create_hist_list(limit);
}

void hist_destroy(void)
{
    destruct_hist_list(history);
}

void hist_add(const char* cmd)
{
    if (history->size >= history->capacity) free(history->commands[history->size % history->capacity]);
    history->commands[history->size % history->capacity] = malloc(strlen(cmd) + 1);
    memcpy(history->commands[history->size % history->capacity], cmd, strlen(cmd) + 1);
    history->size++;
}

void hist_print(void) //oldest first
{
    for (int i = 0; i < MIN(history->size, history->capacity); i++)
        puts(history->commands[(history->size - MIN(history->size, history->capacity) + i) % history->capacity]);
}

const char* hist_search_prefix(char* prefix) //going backwards from most recent command
{
    if (prefix == NULL || prefix[0] == 0) return NULL;

    //LOG("bang search: %s\n", prefix);
    
    int len = strlen(prefix);
    for (int i = 1; i <= MIN(history->size, history->capacity); i++)
    {
        if (memcmp(history->commands[(history->size - i) % history->capacity], prefix, len) == 0) return history->commands[(history->size - i) % history->capacity]; 
    }

    return NULL;
}

const char* hist_search_cnum(int command_number) //is the first command command 0 or command 1? set up for 1, if 0 then take 1 from null check and add 1 inside index
{
    if (command_number < (1 + history->size - MIN(history->size, history->capacity)) || command_number > history->size) return NULL;

    return history->commands[(command_number - 1) % history->capacity];
}

unsigned int hist_last_cnum(void) //set up for 1 being first command (can return 0 if no commands made)
{
    return history->size;
}
