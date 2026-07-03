#include "nextglk.h"
#include <stdio.h>
#include <string.h>

char nextglk_wait_char(void)
{
    return '\0';
}

uint16_t nextglk_read_line(
    char* buffer,
    uint16_t buffer_size)
{
    if (!buffer || buffer_size == 0)
        return 0;

    if (!fgets(buffer, buffer_size, stdin))
        return 0;

    /* Strip trailing newline */
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        buffer[--len] = '\0';

    return (uint16_t)len;
}