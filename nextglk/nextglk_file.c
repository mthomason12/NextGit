#include "nextglk.h"

struct NextGlkFile
{
    int unused;
};

NextGlkFile* nextglk_file_open_read(
    const char* path)
{
    (void)path;

    return 0;
}

NextGlkFile* nextglk_file_open_write(
    const char* path)
{
    (void)path;

    return 0;
}

uint32_t nextglk_file_read(
    NextGlkFile* file,
    void* buffer,
    uint32_t length)
{
    (void)file;
    (void)buffer;
    (void)length;

    return 0;
}

uint32_t nextglk_file_write(
    NextGlkFile* file,
    const void* buffer,
    uint32_t length)
{
    (void)file;
    (void)buffer;
    (void)length;

    return 0;
}

void nextglk_file_close(
    NextGlkFile* file)
{
    (void)file;
}