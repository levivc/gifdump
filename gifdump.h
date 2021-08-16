#ifndef GIFDUMP_H_INCLUDED
#define GIFDUMP_H_INCLUDED

#include <stdio.h>
#include <stdbool.h>

typedef struct
{
    const char* name;
    FILE* file;
} File_io;

typedef struct
{
    File_io input;
    File_io output;
    int index;
    char option;
} Config;

void config_init(Config* config);
void config_dump(Config* config);

bool process_gifstream(Config* config);

#endif
