/* stream_read_tests.c — File stream and stream read API tests
 *
 * Phase 3A.4 — File Stream Open and Close Tests (migrated from fileio_tests.c)
 * Phase 3A.5 — File Stream Output Tests (migrated from fileio_tests.c)
 * Phase 3A.6 — File Stream Position Tests (migrated from fileio_tests.c)
 * Phase 3B.3 — Stream Reading API Tests (char, buffer, line)
 */

#include "../nextglk/nextglk.h"
#include "../nextglk/nextglk_internal.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* =========================================================================
 * Phase 3A.4 — File Stream Open and Close Tests
 * ========================================================================= */

int stream_read_tests_run(void)
{
    /* =====================================================================
     * Phase 3A.4 — File Stream Open and Close
     * ===================================================================== */

    /* ---- open file stream write mode ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4write", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for write stream test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: glk_stream_open_file(filemode_Write) returns non-NULL stream");

        stream_t *s = (stream_t *)str;
        TEST_ASSERT(s->type == strtype_File,
            "3A.4: stream type is strtype_File");
        TEST_ASSERT(s->writable == 1,
            "3A.4: write-mode stream is writable");
        TEST_ASSERT(s->readable == 0,
            "3A.4: write-mode stream is not readable");
        TEST_ASSERT(s->file != NULL,
            "3A.4: stream has non-NULL file pointer");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a4write.glkdata");
    }

    /* ---- open file stream append mode ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Transcript, "sr3a4append", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for append stream test");

        strid_t str = glk_stream_open_file(fref, filemode_WriteAppend, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: glk_stream_open_file(filemode_WriteAppend) returns non-NULL stream");

        stream_t *s = (stream_t *)str;
        TEST_ASSERT(s->type == strtype_File,
            "3A.4: append stream type is strtype_File");
        TEST_ASSERT(s->writable == 1,
            "3A.4: append stream is writable");

        glk_stream_close(str, NULL);

        TEST_ASSERT(access("sr3a4append.txt", F_OK) == 0,
            "3A.4: append stream creates file on disk");

        glk_fileref_destroy(fref);
        unlink("sr3a4append.txt");
    }

    /* ---- NULL fileref returns NULL ---- */

    {
        strid_t str = glk_stream_open_file(NULL, filemode_Write, 0);
        TEST_ASSERT(str == NULL,
            "3A.4: glk_stream_open_file(NULL, ...) returns NULL");
    }

    /* ---- read mode opens successfully ---- */

    {
        NextGlkFile *wf = nextglk_file_open_write("sr3a4read.glkdata");
        TEST_ASSERT(wf != NULL,
            "3A.4: created file on disk for read-mode test");
        nextglk_file_write(wf, "test", 4);
        nextglk_file_close(wf);

        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4read", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for read-mode test");

        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: glk_stream_open_file(filemode_Read) returns non-NULL");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a4read.glkdata");
    }

    /* ---- stream registered in gli_streamlist ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4list", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for stream list test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for list test");

        stream_t *iter = gli_streamlist;
        int found = 0;
        while (iter != NULL)
        {
            if (iter == (stream_t *)str)
            {
                found = 1;
                break;
            }
            iter = iter->next;
        }
        TEST_ASSERT(found == 1,
            "3A.4: file stream is present in gli_streamlist");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a4list.glkdata");
    }

    /* ---- stream removed from list on close ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4removed", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for stream removal test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for removal test");

        glk_stream_close(str, NULL);

        stream_t *iter = gli_streamlist;
        int found = 0;
        while (iter != NULL)
        {
            if (iter == (stream_t *)str)
            {
                found = 1;
                break;
            }
            iter = iter->next;
        }
        TEST_ASSERT(found == 0,
            "3A.4: file stream is removed from gli_streamlist after close");

        glk_fileref_destroy(fref);
        unlink("sr3a4removed.glkdata");
    }

    /* ---- close file stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4close", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for close test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for close test");

        glk_stream_close(str, NULL);
        TEST_ASSERT(1,
            "3A.4: glk_stream_close() on file stream does not crash");

        glk_fileref_destroy(fref);
        unlink("sr3a4close.glkdata");
    }

    /* ---- close current stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4current", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for current stream test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for current stream test");

        glk_stream_set_current(str);
        TEST_ASSERT(glk_stream_get_current() == str,
            "3A.4: stream is current after glk_stream_set_current");

        glk_stream_close(str, NULL);
        TEST_ASSERT(glk_stream_get_current() == NULL,
            "3A.4: current stream is NULL after closing current stream");

        glk_fileref_destroy(fref);
        unlink("sr3a4current.glkdata");
    }

    /* ---- stream rock preserved ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a4rock", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for rock test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 42);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream with rock=42");

        TEST_ASSERT(glk_stream_get_rock(str) == 42,
            "3A.4: glk_stream_get_rock returns 42");

        glk_stream_close(str, NULL);

        str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream with rock=0");
        TEST_ASSERT(glk_stream_get_rock(str) == 0,
            "3A.4: glk_stream_get_rock returns 0 for rock=0 stream");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a4rock.glkdata");
    }

    /* =====================================================================
     * Phase 3A.5 — File Stream Output Tests
     * ===================================================================== */

    /* ---- file stream single character output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5char", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for single char output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for single char output");

        glk_put_char_stream(str, 'X');

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5char.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after single char write");
        int c = fgetc(fp);
        TEST_ASSERT(c == 'X',
            "3A.5: file contains 'X'");
        c = fgetc(fp);
        TEST_ASSERT(c == EOF,
            "3A.5: file contains exactly one byte");
        fclose(fp);
        unlink("sr3a5char.glkdata");
    }

    /* ---- file stream multiple character output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5multichar", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for multiple char output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for multiple char output");

        glk_put_char_stream(str, 'H');
        glk_put_char_stream(str, 'e');
        glk_put_char_stream(str, 'l');
        glk_put_char_stream(str, 'l');
        glk_put_char_stream(str, 'o');

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5multichar.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after multiple char writes");
        char buf[8] = {0};
        size_t n = fread(buf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "3A.5: file contains 5 bytes");
        TEST_ASSERT(strcmp(buf, "Hello") == 0,
            "3A.5: file contents match \"Hello\"");
        fclose(fp);
        unlink("sr3a5multichar.glkdata");
    }

    /* ---- file stream buffer output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5buf", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for buffer output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for buffer output");

        const char *data = "Hello, NextGit World!";
        glk_put_buffer_stream(str, (char *)data, strlen(data));

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5buf.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after buffer write");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == strlen(data),
            "3A.5: file contains correct number of bytes");
        TEST_ASSERT(strcmp(buf, data) == 0,
            "3A.5: file contents match written buffer");
        fclose(fp);
        unlink("sr3a5buf.glkdata");
    }

    /* ---- file stream string output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5str", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for string output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for string output");

        glk_put_string_stream(str, "String via put_string_stream");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5str.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after string write");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == strlen("String via put_string_stream"),
            "3A.5: file contains correct number of bytes");
        TEST_ASSERT(strcmp(buf, "String via put_string_stream") == 0,
            "3A.5: file contents match written string");
        fclose(fp);
        unlink("sr3a5str.glkdata");
    }

    /* ---- writecount updates correctly ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5count", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for writecount test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for writecount test");

        stream_result_t result;
        memset(&result, 0, sizeof(result));

        glk_put_char_stream(str, 'A');
        glk_put_char_stream(str, 'B');
        glk_put_char_stream(str, 'C');
        glk_put_char_stream(str, 'D');

        glk_put_buffer_stream(str, "EFGHI", 5);

        glk_put_string_stream(str, "JKLMNO");

        glk_stream_close(str, &result);

        TEST_ASSERT(result.writecount == 15,
            "3A.5: writecount is 15 (4 chars + 5 buffer + 6 string)");
        TEST_ASSERT(result.readcount == 0,
            "3A.5: readcount is 0 (write-only stream)");

        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5count.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after writecount test");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 15,
            "3A.5: file contains 15 bytes");
        TEST_ASSERT(strcmp(buf, "ABCDEFGHIJKLMNO") == 0,
            "3A.5: file contents match expected concatenation");
        fclose(fp);
        unlink("sr3a5count.glkdata");
    }

    /* ---- empty buffer output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5empty", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for empty buffer test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for empty buffer test");

        glk_put_buffer_stream(str, "data", 0);

        stream_result_t result;
        memset(&result, 0, sizeof(result));
        glk_stream_close(str, &result);

        TEST_ASSERT(result.writecount == 0,
            "3A.5: writecount is 0 after empty buffer write");

        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5empty.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: file exists after empty buffer write");
        long file_size;
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        TEST_ASSERT(file_size == 0,
            "3A.5: file is empty (0 bytes)");
        fclose(fp);
        unlink("sr3a5empty.glkdata");
    }

    /* ---- multiple writes to the same stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5multi", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for multiple writes test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for multiple writes");

        glk_put_char_stream(str, '[');
        glk_put_buffer_stream(str, "chunk1", 6);
        glk_put_char_stream(str, '|');
        glk_put_string_stream(str, "chunk2");
        glk_put_char_stream(str, ']');

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5multi.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after multiple writes");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 15,
            "3A.5: file contains 15 bytes");
        TEST_ASSERT(strcmp(buf, "[chunk1|chunk2]") == 0,
            "3A.5: file contents match expected pattern");
        fclose(fp);
        unlink("sr3a5multi.glkdata");
    }

    /* ---- append stream output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Transcript, "sr3a5appendout", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for append output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream (write mode) for append test");
        glk_put_string_stream(str, "Line 1\n");
        glk_stream_close(str, NULL);

        str = glk_stream_open_file(fref, filemode_WriteAppend, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: reopened file stream (append mode) for append test");
        glk_put_string_stream(str, "Line 2\n");
        glk_stream_close(str, NULL);

        str = glk_stream_open_file(fref, filemode_WriteAppend, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: reopened file stream (append mode) for second append");
        glk_put_string_stream(str, "Line 3\n");
        glk_stream_close(str, NULL);

        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5appendout.txt", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open appended file for reading");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 21,
            "3A.5: appended file contains 21 bytes");
        TEST_ASSERT(strcmp(buf, "Line 1\nLine 2\nLine 3\n") == 0,
            "3A.5: appended file contains all three lines");
        fclose(fp);
        unlink("sr3a5appendout.txt");
    }

    /* ---- binary data write via glk_put_buffer_stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a5binary", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for binary output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for binary output");

        const unsigned char bin[] = { 0x00, 0xFF, 'A', 0x7F, 0x80 };
        glk_put_buffer_stream(str, (const char *)bin, 5);

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a5binary.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open binary file for reading");
        unsigned char bbuf[8] = {0};
        size_t n = fread(bbuf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "3A.5: binary file contains 5 bytes");
        TEST_ASSERT(bbuf[0] == 0x00 && bbuf[1] == 0xFF && bbuf[2] == 'A'
            && bbuf[3] == 0x7F && bbuf[4] == 0x80,
            "3A.5: binary data preserved correctly including 0x00 and 0xFF");
        fclose(fp);
        unlink("sr3a5binary.glkdata");
    }

    /* =====================================================================
     * Phase 3A.6 — File Stream Position Tests
     * ===================================================================== */

    /* ---- get position on empty file ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6empty", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for empty-file position test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for empty-file position test");

        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 on newly-opened empty file");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a6empty.glkdata");
    }

    /* ---- position after writes ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6poswrite", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for position-after-writes test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for position-after-writes test");

        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 before any write");

        glk_put_string_stream(str, "Hello");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after writing \"Hello\"");

        glk_put_string_stream(str, "World");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after writing \"HelloWorld\"");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a6poswrite.glkdata");
    }

    /* ---- seek to start ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6seekstart", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for seek-to-start test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for seek-to-start test");

        glk_put_string_stream(str, "abcdefgh");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 8,
            "3A.6: position is 8 after writing 8 bytes");

        glk_stream_set_position(str, 0, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 after seekmode_Start to 0");

        glk_stream_set_position(str, 4, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 4,
            "3A.6: position is 4 after seekmode_Start to 4");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a6seekstart.glkdata");
    }

    /* ---- seek relative to current ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6seekcur", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for seek-relative test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for seek-relative test");

        glk_put_string_stream(str, "Hello");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after writing \"Hello\"");

        glk_stream_set_position(str, -2, seekmode_Current);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 3,
            "3A.6: position is 3 after seekmode_Current to -2");

        glk_stream_set_position(str, 4, seekmode_Current);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 7,
            "3A.6: position is 7 after seekmode_Current to +4");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a6seekcur.glkdata");
    }

    /* ---- seek relative to end ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6seekend", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for seek-from-end test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for seek-from-end test");

        glk_put_string_stream(str, "0123456789");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after writing 10 bytes");

        glk_stream_set_position(str, -3, seekmode_End);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 7,
            "3A.6: position is 7 after seekmode_End to -3");

        glk_stream_set_position(str, 0, seekmode_End);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after seekmode_End to 0");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a6seekend.glkdata");
    }

    /* ---- overwrite existing bytes after seek ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6overwrite", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for overwrite-after-seek test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for overwrite-after-seek test");

        glk_put_string_stream(str, "Hello");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after writing \"Hello\"");

        glk_stream_set_position(str, 0, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 after seek to start");

        glk_put_string_stream(str, "XYZ");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 3,
            "3A.6: position is 3 after writing \"XYZ\"");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a6overwrite.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.6: can open file for reading after overwrite");
        char obuf[16] = {0};
        size_t n = fread(obuf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "3A.6: file contains 5 bytes after overwrite");
        TEST_ASSERT(strcmp(obuf, "XYZlo") == 0,
            "3A.6: file content is \"XYZlo\" after overwriting first 3 bytes of \"Hello\"");
        fclose(fp);
        unlink("sr3a6overwrite.glkdata");
    }

    /* ---- multiple seek operations ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6multiseek", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for multi-seek test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for multi-seek test");

        glk_put_string_stream(str, "AAAAABBBBBCCCCC");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 15,
            "3A.6: position is 15 after writing 15 bytes");

        glk_stream_set_position(str, 5, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after seek to 5");
        glk_put_string_stream(str, "XXXXX");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after writing \"XXXXX\"");

        glk_stream_set_position(str, 10, seekmode_Start);
        glk_put_string_stream(str, "YYYYY");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 15,
            "3A.6: position is 15 after writing \"YYYYY\"");

        glk_stream_set_position(str, 0, seekmode_Start);
        glk_put_string_stream(str, "ZZZZZ");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        FILE *fp = fopen("sr3a6multiseek.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.6: can open file for reading after multi-seek");
        char mbuf[32] = {0};
        size_t n = fread(mbuf, 1, 15, fp);
        TEST_ASSERT(n == 15,
            "3A.6: file contains 15 bytes after multi-seek");
        TEST_ASSERT(strcmp(mbuf, "ZZZZZXXXXXYYYYY") == 0,
            "3A.6: file content is \"ZZZZZXXXXXYYYYY\" after multi-seek overwrites");
        fclose(fp);
        unlink("sr3a6multiseek.glkdata");
    }

    /* ---- invalid stream safety (NULL stream) ---- */

    {
        glui32 pos = glk_stream_get_position(NULL);
        TEST_ASSERT(pos == 0,
            "3A.6: glk_stream_get_position(NULL) returns 0");

        glk_stream_set_position(NULL, 0, seekmode_Start);
        TEST_ASSERT(1,
            "3A.6: glk_stream_set_position(NULL, ...) does not crash");
    }

    /* ---- file position persistence -- reopened file starts at 0 ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "sr3a6persist", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for position persistence test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream (first session)");

        glk_put_string_stream(str, "Session One Data");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 16,
            "3A.6: position is 16 after writing 16 bytes in first session");

        glk_stream_close(str, NULL);

        str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: reopened file stream (second session)");

        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 after reopening file (truncated)");

        glk_put_string_stream(str, "NewData");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 7,
            "3A.6: position is 7 after writing 7 bytes in second session");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("sr3a6persist.glkdata");
    }

    /* =====================================================================
     * Phase 3B.3 — Stream Reading API Tests
     * ===================================================================== */

#define TEST_3B3_BASE   "/tmp/nextgit-test-3b3"

    /* ---- Test 1: Read character from file ---- */

    {
        /* Create a file with known content */
        FILE *fp = fopen(TEST_3B3_BASE "-char.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for char read");
        fputc('X', fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-char.bin",
            fileusage_Data, 0);
        TEST_ASSERT(fref != NULL, "3B.3: fileref created for char read");

        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for char read");

        glsi32 ch = glk_get_char_stream(str);
        TEST_ASSERT(ch == 'X', "3B.3: glk_get_char_stream returns 'X' (88)");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-char.bin");
    }

    /* ---- Test 2: Read multiple characters sequentially ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-seq.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for sequential read");
        fputs("Hello", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-seq.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for sequential read");

        TEST_ASSERT(glk_get_char_stream(str) == 'H', "3B.3: char 1 is 'H'");
        TEST_ASSERT(glk_get_char_stream(str) == 'e', "3B.3: char 2 is 'e'");
        TEST_ASSERT(glk_get_char_stream(str) == 'l', "3B.3: char 3 is 'l'");
        TEST_ASSERT(glk_get_char_stream(str) == 'l', "3B.3: char 4 is 'l'");
        TEST_ASSERT(glk_get_char_stream(str) == 'o', "3B.3: char 5 is 'o'");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-seq.bin");
    }

    /* ---- Test 3: Character EOF returns -1 ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-eof.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for EOF test");
        fputc('A', fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-eof.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for EOF test");

        glsi32 ch = glk_get_char_stream(str);
        TEST_ASSERT(ch == 'A', "3B.3: first char is 'A'");

        ch = glk_get_char_stream(str);
        TEST_ASSERT(ch == -1, "3B.3: second char returns -1 (EOF)");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-eof.bin");
    }

    /* ---- Test 4: Buffer read entire file ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-fullbuf.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for buffer read");
        fputs("Full file content here.", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-fullbuf.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for buffer read");

        char buf[64] = {0};
        glui32 n = glk_get_buffer_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 23, "3B.3: buffer read returns 23 bytes");
        TEST_ASSERT(strcmp(buf, "Full file content here.") == 0,
            "3B.3: buffer content matches");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-fullbuf.bin");
    }

    /* ---- Test 5: Buffer read partial file ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-partbuf.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for partial buffer read");
        fputs("ABCDEFGHIJ", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-partbuf.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for partial buffer read");

        char buf[8] = {0};
        glui32 n = glk_get_buffer_stream(str, buf, 4);
        TEST_ASSERT(n == 4, "3B.3: first partial read returns 4 bytes");
        TEST_ASSERT(memcmp(buf, "ABCD", 4) == 0, "3B.3: first chunk is \"ABCD\"");

        n = glk_get_buffer_stream(str, buf, 4);
        TEST_ASSERT(n == 4, "3B.3: second partial read returns 4 bytes");
        TEST_ASSERT(memcmp(buf, "EFGH", 4) == 0, "3B.3: second chunk is \"EFGH\"");

        n = glk_get_buffer_stream(str, buf, 4);
        TEST_ASSERT(n == 2, "3B.3: third partial read returns remaining 2 bytes");
        TEST_ASSERT(memcmp(buf, "IJ", 2) == 0, "3B.3: third chunk is \"IJ\"");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-partbuf.bin");
    }

    /* ---- Test 6: Zero-length buffer read ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-zerolen.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for zero-length read");
        fputs("data", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-zerolen.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for zero-length read");

        glui32 n = glk_get_buffer_stream(str, (char *)"dummy", 0);
        TEST_ASSERT(n == 0, "3B.3: zero-length buffer read returns 0");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-zerolen.bin");
    }

    /* ---- Test 7: NULL stream buffer read ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-nullbuf.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for NULL buffer read");
        fputs("data", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-nullbuf.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for NULL buffer read");

        glui32 n = glk_get_buffer_stream(str, NULL, 4);
        TEST_ASSERT(n == 0, "3B.3: NULL buffer read returns 0");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-nullbuf.bin");
    }

    /* ---- Test 8: Line read single line ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-oneline.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for single line read");
        fputs("Hello, World!\n", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-oneline.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for single line read");

        char buf[64] = {0};
        glui32 n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 14, "3B.3: line read returns 14 bytes (including newline)");
        TEST_ASSERT(strcmp(buf, "Hello, World!\n") == 0,
            "3B.3: line content matches, NUL-terminated");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-oneline.bin");
    }

    /* ---- Test 9: Line read multiple lines ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-multiline.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for multi-line read");
        fputs("Line1\nLine2\nLine3\n", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-multiline.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for multi-line read");

        char buf[64] = {0};

        glui32 n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 6, "3B.3: first line read returns 6 bytes");
        TEST_ASSERT(strcmp(buf, "Line1\n") == 0, "3B.3: first line is \"Line1\\n\"");

        n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 6, "3B.3: second line read returns 6 bytes");
        TEST_ASSERT(strcmp(buf, "Line2\n") == 0, "3B.3: second line is \"Line2\\n\"");

        n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 6, "3B.3: third line read returns 6 bytes");
        TEST_ASSERT(strcmp(buf, "Line3\n") == 0, "3B.3: third line is \"Line3\\n\"");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-multiline.bin");
    }

    /* ---- Test 10: Line EOF handling ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-lineeof.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for line EOF");
        fputs("No trailing newline", fp);  /* no \n */
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-lineeof.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for line EOF");

        char buf[64] = {0};
        glui32 n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 19, "3B.3: line with no newline returns all bytes");
        TEST_ASSERT(strcmp(buf, "No trailing newline") == 0,
            "3B.3: content matches, NUL-terminated");

        /* Second read should return 0 (EOF) */
        n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 0, "3B.3: second line read returns 0 (EOF)");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-lineeof.bin");
    }

    /* ---- Test 11: Empty file behaviour ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-empty.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create empty test file");
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-empty.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for empty file");

        /* char read on empty */
        glsi32 ch = glk_get_char_stream(str);
        TEST_ASSERT(ch == -1, "3B.3: char read on empty file returns -1");

        glk_stream_close(str, NULL);

        /* Reopen for buffer/line tests */
        str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream reopened for empty file buffer test");

        char buf[8] = {0};
        glui32 n = glk_get_buffer_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 0, "3B.3: buffer read on empty file returns 0");

        glk_stream_close(str, NULL);

        str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream reopened for empty file line test");

        n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 0, "3B.3: line read on empty file returns 0");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-empty.bin");
    }

    /* ---- Test 12: Position tracking after char reads ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-poschar.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for char position");
        fputs("ABC", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-poschar.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str != NULL, "3B.3: stream opened for char position");

        TEST_ASSERT(glk_stream_get_position(str) == 0,
            "3B.3: position is 0 before reads");

        glk_get_char_stream(str);
        TEST_ASSERT(glk_stream_get_position(str) == 1,
            "3B.3: position is 1 after one char read");

        glk_get_char_stream(str);
        TEST_ASSERT(glk_stream_get_position(str) == 2,
            "3B.3: position is 2 after two char reads");

        glk_get_char_stream(str);
        TEST_ASSERT(glk_stream_get_position(str) == 3,
            "3B.3: position is 3 after three char reads");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-poschar.bin");
    }

    /* ---- Test 13: Position tracking after buffer reads ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-posbuf.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for buffer position");
        fputs("0123456789", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-posbuf.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        char buf[8] = {0};
        glui32 n = glk_get_buffer_stream(str, buf, 4);
        TEST_ASSERT(n == 4, "3B.3: first buffer read returns 4 bytes");
        TEST_ASSERT(glk_stream_get_position(str) == 4,
            "3B.3: position is 4 after reading 4 bytes");

        n = glk_get_buffer_stream(str, buf, 6);
        TEST_ASSERT(n == 6, "3B.3: second buffer read returns 6 bytes");
        TEST_ASSERT(glk_stream_get_position(str) == 10,
            "3B.3: position is 10 after reading to EOF");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-posbuf.bin");
    }

    /* ---- Test 14: Position tracking after line reads ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-posline.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for line position");
        fputs("ABC\nDEF\n", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-posline.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        char buf[32] = {0};
        glui32 n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 4, "3B.3: first line returns 4 bytes (\"ABC\\n\")");
        TEST_ASSERT(glk_stream_get_position(str) == 4,
            "3B.3: position is 4 after first line");

        n = glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(n == 4, "3B.3: second line returns 4 bytes (\"DEF\\n\")");
        TEST_ASSERT(glk_stream_get_position(str) == 8,
            "3B.3: position is 8 after second line");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-posline.bin");
    }

    /* ---- Test 15: Readcount tracking ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-readcount.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for readcount");
        fputs("ABCDEFGHIJ", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-readcount.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        stream_t *s = (stream_t *)str;
        TEST_ASSERT(s->readcount == 0, "3B.3: initial readcount is 0");

        glk_get_char_stream(str);
        TEST_ASSERT(s->readcount == 1, "3B.3: readcount is 1 after 1 char");

        glk_get_char_stream(str);
        TEST_ASSERT(s->readcount == 2, "3B.3: readcount is 2 after 2 chars");

        char buf[8] = {0};
        glk_get_buffer_stream(str, buf, 3);
        TEST_ASSERT(s->readcount == 5, "3B.3: readcount is 5 after char+char+buffer(3)");

        glk_get_line_stream(str, buf, sizeof(buf));
        TEST_ASSERT(s->readcount == 10,
            "3B.3: readcount is 10 after char+char+buffer(3)+line(remaining 5)");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-readcount.bin");
    }

    /* ---- Test 16: Simultaneous streams maintain independent positions ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-indep1.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create first file for independence test");
        fputs("AAA", fp);
        fclose(fp);

        fp = fopen(TEST_3B3_BASE "-indep2.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create second file for independence test");
        fputs("BBBBBB", fp);
        fclose(fp);

        fileref_t *fref1 = gli_new_fileref(TEST_3B3_BASE "-indep1.bin",
            fileusage_Data, 0);
        fileref_t *fref2 = gli_new_fileref(TEST_3B3_BASE "-indep2.bin",
            fileusage_Data, 0);
        strid_t str1 = glk_stream_open_file(fref1, filemode_Read, 0);
        strid_t str2 = glk_stream_open_file(fref2, filemode_Read, 0);

        /* Read one char from str1 */
        glk_get_char_stream(str1);
        TEST_ASSERT(glk_stream_get_position(str1) == 1, "3B.3: str1 position is 1");
        TEST_ASSERT(glk_stream_get_position(str2) == 0, "3B.3: str2 position is still 0");

        /* Read one char from str2 */
        glk_get_char_stream(str2);
        TEST_ASSERT(glk_stream_get_position(str1) == 1, "3B.3: str1 position unchanged");
        TEST_ASSERT(glk_stream_get_position(str2) == 1, "3B.3: str2 position is 1");

        /* Read remaining from both */
        char buf[8] = {0};
        glk_get_buffer_stream(str1, buf, 2);
        TEST_ASSERT(glk_stream_get_position(str1) == 3, "3B.3: str1 position is 3 at EOF");
        TEST_ASSERT(glk_stream_get_position(str2) == 1, "3B.3: str2 position still 1");

        glk_get_buffer_stream(str2, buf, 5);
        TEST_ASSERT(glk_stream_get_position(str2) == 6, "3B.3: str2 position is 6 at EOF");

        glk_stream_close(str1, NULL);
        glk_stream_close(str2, NULL);
        glk_fileref_destroy(fref1);
        glk_fileref_destroy(fref2);
        unlink(TEST_3B3_BASE "-indep1.bin");
        unlink(TEST_3B3_BASE "-indep2.bin");
    }

    /* ---- Test 17: Binary data survives buffer reads unchanged ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-binsurvive.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create binary test file");
        const unsigned char bin[] = { 0x00, 0xFF, 'X', 0x7F, 0x80, 0x55 };
        fwrite(bin, 1, sizeof(bin), fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-binsurvive.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        unsigned char rbuf[16] = {0};
        glui32 n = glk_get_buffer_stream(str, (char *)rbuf, sizeof(rbuf));
        TEST_ASSERT(n == sizeof(bin), "3B.3: binary buffer read returns all bytes");
        TEST_ASSERT(memcmp(rbuf, bin, sizeof(bin)) == 0,
            "3B.3: binary data survives round-trip unchanged (0x00, 0xFF preserved)");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-binsurvive.bin");
    }

    /* ---- Test 18: len==1 line buffer behaviour ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-len1.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for len==1 line read");
        fputs("Hello\n", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-len1.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        /* len==1: cannot NUL-terminate.  Reads 'H', buffer full, returns 1. */
        char buf[1];
        glui32 n = glk_get_line_stream(str, buf, 1);
        TEST_ASSERT(n == 1, "3B.3: len==1 line read returns 1 byte");
        TEST_ASSERT(buf[0] == 'H', "3B.3: len==1 buffer contains 'H'");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-len1.bin");
    }

    /* ---- Test 19: Repeated EOF reads remain safe ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-repeateof.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for repeated EOF");
        fputc('Z', fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-repeateof.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        /* Read the only byte */
        TEST_ASSERT(glk_get_char_stream(str) == 'Z', "3B.3: first char read is 'Z'");

        /* Repeated EOF reads */
        for (int i = 0; i < 5; i++)
        {
            glsi32 ch = glk_get_char_stream(str);
            TEST_ASSERT(ch == -1, "3B.3: repeated EOF read returns -1");
        }

        /* Buffer read at EOF */
        char buf[8];
        TEST_ASSERT(glk_get_buffer_stream(str, buf, sizeof(buf)) == 0,
            "3B.3: buffer read at EOF returns 0");

        /* Line read at EOF */
        TEST_ASSERT(glk_get_line_stream(str, buf, sizeof(buf)) == 0,
            "3B.3: line read at EOF returns 0");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-repeateof.bin");
    }

    /* ---- Test 20: File stream close after reads ---- */

    {
        FILE *fp = fopen(TEST_3B3_BASE "-closeafter.bin", "wb");
        TEST_ASSERT(fp != NULL, "3B.3: setup: create test file for close-after-read");
        fputs("Close after read test data.", fp);
        fclose(fp);

        fileref_t *fref = gli_new_fileref(TEST_3B3_BASE "-closeafter.bin",
            fileusage_Data, 0);
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);

        /* Do some mixed reads */
        glsi32 ch = glk_get_char_stream(str);
        TEST_ASSERT(ch == 'C', "3B.3: char read before close");

        char buf[8] = {0};
        glui32 n = glk_get_buffer_stream(str, buf, 4);
        TEST_ASSERT(n == 4, "3B.3: buffer read before close");

        stream_result_t result;
        memset(&result, 0, sizeof(result));
        glk_stream_close(str, &result);

        /* readcount should reflect all bytes read (1 char + 4 buffer) */
        TEST_ASSERT(result.readcount == 5,
            "3B.3: readcount is 5 after 1 char + 4 buffer reads");

        glk_fileref_destroy(fref);
        unlink(TEST_3B3_BASE "-closeafter.bin");
    }

#undef TEST_3B3_BASE

    return 0;
}