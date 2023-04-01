#include "commands.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int cd(char * args[])
{
    args = args + 1;

    if (args[0] == NULL)
    {
        fprintf(stderr, "usage: cd <dest>\n");
        return 1;
    }

    // parse ~, ., ..
    
    chdir(args[0]);

    return -1;
}