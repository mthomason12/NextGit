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
    NextGlkFile* nf;

    if (!path)
        return NULL;

    nf = malloc(sizeof(*nf));
    if (!nf)
        return NULL;

    nf->fp = fopen(path, "rb");
    if (!nf->fp)
    {
        free(nf);
        return NULL;
    }

    return nf;
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
    size_t n;

    if (!file || !file->fp || !buffer)
        return 0;

    if (length == 0)
        return 0;

    n = fread(buffer, 1, length, file->fp);
    return (uint32_t)n;
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

uint32_t nextglk_file_get_position(
    NextGlkFile* file)
{
    long pos;

    if (!file || !file->fp)
        return 0;

    pos = ftell(file->fp);
    return (pos >= 0) ? (uint32_t)pos : 0;
}

void nextglk_file_set_position(
    NextGlkFile* file,
    int32_t pos,
    int whence)
{
    if (!file || !file->fp)
        return;

    fseek(file->fp, (long)pos, whence);
}
