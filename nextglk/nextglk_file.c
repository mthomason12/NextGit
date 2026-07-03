#include "nextglk.h"
#include <stdio.h>
#include <stdlib.h>

struct NextGlkFile
{
    FILE *fp;
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
    NextGlkFile* nf;

    if (!path)
        return NULL;

    nf = malloc(sizeof(*nf));
    if (!nf)
        return NULL;

    nf->fp = fopen(path, "wb");
    if (!nf->fp)
    {
        free(nf);
        return NULL;
    }

    return nf;
}

NextGlkFile* nextglk_file_append(
    const char* path)
{
    NextGlkFile* nf;

    if (!path)
        return NULL;

    nf = malloc(sizeof(*nf));
    if (!nf)
        return NULL;

    nf->fp = fopen(path, "ab");
    if (!nf->fp)
    {
        free(nf);
        return NULL;
    }

    return nf;
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
    size_t written;

    if (!file || !file->fp || !buffer)
        return 0;

    if (length == 0)
        return 0;

    written = fwrite(buffer, 1, length, file->fp);
    return (uint32_t)written;
}

void nextglk_file_close(
    NextGlkFile* file)
{
    if (!file)
        return;

    if (file->fp)
        fclose(file->fp);

    free(file);
}