#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

size_t strip(char * str)
{
    size_t n = strlen(str);

    // right strip
    while (n > 0 && str[n - 1] == ' ') n--;
    str[n] = '\0';

    // left strip
    int i = 0;
    while (i < n && str[i] == ' ') i++;

    for (int j = 0; j < (n - i + 1); j++)
    {
        str[j] = str[i + j];
    }

    return n;
}