/* fileio_tests.c — Fileref lifecycle and creation tests for Phase 3A
 *
 * Phase 3A.1 tests:
 *   - fileref allocation via gli_new_fileref()
 *   - fileref destruction via gli_delete_fileref()
 *   - list insertion (head insertion)
 *   - list removal from head, middle, tail
 *   - iteration via glk_fileref_iterate()
 *   - rock value storage and retrieval
 *   - dispatch registration callback on fileref creation
 *   - dispatch unregistration callback on fileref destruction
 *   - glk_fileref_destroy() public API
 *
 * Phase 3A.2 tests:
 *   - glk_fileref_create_by_name() suffix handling
 *   - glk_fileref_create_by_name() no double-extension
 *   - glk_fileref_create_by_name() with NULL name
 *   - glk_fileref_create_by_prompt() fixed filenames
 *   - glk_fileref_create_temp() creates unique file
 *   - glk_fileref_create_from_fileref() clones filename
 *   - glk_fileref_create_from_fileref() changes usage
 *   - glk_fileref_create_from_fileref() with NULL
 *   - Iteration after creation (all types)
 *   - glk_fileref_destroy() after creation
 */

#include "../nextglk/nextglk.h"
#include "../nextglk/nextglk_internal.h"
#include "test_common.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* -------------------------------------------------------------------------
 * Mock callback state
 * ------------------------------------------------------------------------- */

static int mock_regi_called = 0;
static void *mock_regi_obj = NULL;
static glui32 mock_regi_class = 0xFFFFFFFF;
static gidispatch_rock_t mock_regi_result;

static int mock_unregi_called = 0;
static void *mock_unregi_obj = NULL;
static glui32 mock_unregi_class = 0xFFFFFFFF;
static gidispatch_rock_t mock_unregi_rock;

/* Reset all mock state to defaults */
static void mock_reset(void)
{
    mock_regi_called = 0;
    mock_regi_obj = NULL;
    mock_regi_class = 0xFFFFFFFF;
    mock_regi_result.num = 0;

    mock_unregi_called = 0;
    mock_unregi_obj = NULL;
    mock_unregi_class = 0xFFFFFFFF;
    mock_unregi_rock.num = 0;
}

/* Mock registration callback — records arguments and returns a known rock */
static gidispatch_rock_t mock_register_obj(void *obj, glui32 objclass)
{
    mock_regi_called++;
    mock_regi_obj = obj;
    mock_regi_class = objclass;
    return mock_regi_result;
}

/* Mock unregistration callback — records arguments */
static void mock_unregister_obj(void *obj, glui32 objclass,
    gidispatch_rock_t objrock)
{
    mock_unregi_called++;
    mock_unregi_obj = obj;
    mock_unregi_class = objclass;
    mock_unregi_rock = objrock;
}

/* =========================================================================
 * Test runner
 * ========================================================================= */

int fileio_tests_run(void)
{
    /* =====================================================================
     * Phase 3A.1 — Fileref Lifecycle Tests
     * ===================================================================== */

    /* ---- Fileref allocation via gli_new_fileref ---- */

    {
        fileref_t *f = gli_new_fileref("testfile.txt", 0, 0);
        TEST_ASSERT(f != NULL,
            "gli_new_fileref() creates a fileref");
        TEST_ASSERT(f->filename != NULL,
            "fileref has a filename pointer");
        TEST_ASSERT(strcmp(f->filename, "testfile.txt") == 0,
            "fileref filename matches the input");
        TEST_ASSERT(f->filename != (char*)"testfile.txt",
            "fileref filename is a copy, not the original pointer");
        TEST_ASSERT(f->rock == 0,
            "fileref rock is 0 (as set)");
        TEST_ASSERT(f->usage == 0,
            "fileref usage is 0 (as set)");
        gli_delete_fileref(f);
    }

    /* ---- Fileref with non-zero rock and usage ---- */

    {
        fileref_t *f = gli_new_fileref("data.glkdata", 0x02, 12345);
        TEST_ASSERT(f != NULL,
            "gli_new_fileref() creates fileref with rock and usage set");
        TEST_ASSERT(f->rock == 12345,
            "fileref rock stored correctly");
        TEST_ASSERT(f->usage == 0x02,
            "fileref usage stored correctly");
        gli_delete_fileref(f);
    }

    /* ---- gli_new_fileref with NULL filename returns NULL ---- */

    {
        fileref_t *f = gli_new_fileref(NULL, 0, 0);
        TEST_ASSERT(f == NULL,
            "gli_new_fileref(NULL) returns NULL");
    }

    /* ---- List insertion: single fileref becomes head ---- */

    {
        fileref_t *f = gli_new_fileref("head.gls", 0, 10);
        TEST_ASSERT(gli_filereflist == f,
            "First fileref becomes gli_filereflist head");
        TEST_ASSERT(f->prev == NULL,
            "Head fileref has NULL prev");
        TEST_ASSERT(f->next == NULL,
            "Single fileref has NULL next");
        gli_delete_fileref(f);
        TEST_ASSERT(gli_filereflist == NULL,
            "gli_filereflist is NULL after deleting only fileref");
    }

    /* ---- List insertion: second fileref becomes new head ---- */

    {
        fileref_t *f1 = gli_new_fileref("first.gls", 0, 1);
        fileref_t *f2 = gli_new_fileref("second.gls", 0, 2);

        TEST_ASSERT(gli_filereflist == f2,
            "Second fileref becomes new head of gli_filereflist");
        TEST_ASSERT(f2->next == f1,
            "New head points to previous head");
        TEST_ASSERT(f1->prev == f2,
            "Previous head's prev points to new head");
        TEST_ASSERT(f2->prev == NULL,
            "New head has NULL prev");
        TEST_ASSERT(f1->next == NULL,
            "Old head has NULL next (tail)");

        gli_delete_fileref(f1);
        gli_delete_fileref(f2);
    }

    /* ---- List removal: remove head (gli_filereflist updated) ---- */

    {
        fileref_t *f1 = gli_new_fileref("first.gls", 0, 1);
        fileref_t *f2 = gli_new_fileref("second.gls", 0, 2);

        TEST_ASSERT(gli_filereflist == f2,
            "Head is f2 before removal");

        /* Remove f2 (head) */
        gli_delete_fileref(f2);

        TEST_ASSERT(gli_filereflist == f1,
            "gli_filereflist updated to f1 after head removal");
        TEST_ASSERT(f1->prev == NULL,
            "f1 has NULL prev after head removed");

        gli_delete_fileref(f1);
    }

    /* ---- List removal: remove tail ---- */

    {
        fileref_t *f1 = gli_new_fileref("a.gls", 0, 1);
        fileref_t *f2 = gli_new_fileref("b.gls", 0, 2);
        fileref_t *f3 = gli_new_fileref("c.gls", 0, 3);
        /* List order: f3 -> f2 -> f1 */

        TEST_ASSERT(gli_filereflist == f3,
            "Head is f3");
        TEST_ASSERT(f1->next == NULL,
            "f1 is tail (next is NULL)");

        /* Remove f1 (tail) */
        gli_delete_fileref(f1);

        TEST_ASSERT(f2->next == NULL,
            "f2 is now tail (next is NULL) after tail removal");

        gli_delete_fileref(f2);
        gli_delete_fileref(f3);
    }

    /* ---- List removal: remove middle ---- */

    {
        fileref_t *f1 = gli_new_fileref("a.gls", 0, 1);
        fileref_t *f2 = gli_new_fileref("b.gls", 0, 2);
        fileref_t *f3 = gli_new_fileref("c.gls", 0, 3);
        /* List order: f3 -> f2 -> f1 */

        /* Remove f2 (middle) */
        gli_delete_fileref(f2);

        TEST_ASSERT(f3->next == f1,
            "After middle removal, f3->next is f1");
        TEST_ASSERT(f1->prev == f3,
            "After middle removal, f1->prev is f3");

        gli_delete_fileref(f3);
        gli_delete_fileref(f1);
    }

    /* ---- Iteration: empty list ---- */

    {
        /* Ensure gli_filereflist is empty */
        TEST_ASSERT(gli_filereflist == NULL,
            "gli_filereflist is NULL (no filerefs)");

        fileref_t *f = glk_fileref_iterate(NULL, NULL);
        TEST_ASSERT(f == NULL,
            "glk_fileref_iterate(NULL) returns NULL on empty list");
    }

    /* ---- Iteration: single fileref ---- */

    {
        glui32 rock = 999;
        fileref_t *f1 = gli_new_fileref("single.gls", 0, 42);

        fileref_t *f = glk_fileref_iterate(NULL, &rock);
        TEST_ASSERT(f == f1,
            "glk_fileref_iterate(NULL) returns the only fileref");
        TEST_ASSERT(rock == 42,
            "rockptr receives correct rock value (42)");

        f = glk_fileref_iterate(f1, &rock);
        TEST_ASSERT(f == NULL,
            "glk_fileref_iterate(f1) returns NULL (no more filerefs)");
        TEST_ASSERT(rock == 0,
            "rockptr receives 0 when iteration ends");

        gli_delete_fileref(f1);
    }

    /* ---- Iteration: multiple filerefs ---- */

    {
        glui32 rock;
        fileref_t *f1 = gli_new_fileref("one.gls", 0, 10);
        fileref_t *f2 = gli_new_fileref("two.gls", 0, 20);
        fileref_t *f3 = gli_new_fileref("three.gls", 0, 30);
        /* List order (head insertion): f3 -> f2 -> f1 */

        /* First call */
        rock = 0;
        fileref_t *f = glk_fileref_iterate(NULL, &rock);
        TEST_ASSERT(f == f3,
            "First iterate returns head (f3)");
        TEST_ASSERT(rock == 30,
            "First iterate rock is 30");

        /* Second call */
        f = glk_fileref_iterate(f, &rock);
        TEST_ASSERT(f == f2,
            "Second iterate returns f2");
        TEST_ASSERT(rock == 20,
            "Second iterate rock is 20");

        /* Third call */
        f = glk_fileref_iterate(f, &rock);
        TEST_ASSERT(f == f1,
            "Third iterate returns f1 (tail)");
        TEST_ASSERT(rock == 10,
            "Third iterate rock is 10");

        /* End of iteration */
        f = glk_fileref_iterate(f, &rock);
        TEST_ASSERT(f == NULL,
            "Fourth iterate returns NULL");
        TEST_ASSERT(rock == 0,
            "rockptr receives 0 at end");

        gli_delete_fileref(f3);
        gli_delete_fileref(f2);
        gli_delete_fileref(f1);
    }

    /* ---- Iteration with NULL rockptr ---- */

    {
        fileref_t *f1 = gli_new_fileref("test.gls", 0, 99);

        fileref_t *f = glk_fileref_iterate(NULL, NULL);
        TEST_ASSERT(f == f1,
            "glk_fileref_iterate(NULL, NULL) works without rockptr");

        gli_delete_fileref(f1);
    }

    /* ---- glk_fileref_get_rock ---- */

    {
        fileref_t *f = gli_new_fileref("rocktest.gls", 0, 777);
        TEST_ASSERT(glk_fileref_get_rock(f) == 777,
            "glk_fileref_get_rock returns correct rock value (777)");
        gli_delete_fileref(f);
    }

    /* ---- glk_fileref_get_rock with NULL fileref ---- */

    {
        TEST_ASSERT(glk_fileref_get_rock(NULL) == 0,
            "glk_fileref_get_rock(NULL) returns 0");
    }

    /* ---- glk_fileref_destroy (public API) ---- */

    {
        fileref_t *f = gli_new_fileref("destroy.gls", 0, 0);
        TEST_ASSERT(f != NULL,
            "Created fileref for destroy test");
        TEST_ASSERT(gli_filereflist == f,
            "Fileref is in list before destroy");

        glk_fileref_destroy(f);

        TEST_ASSERT(gli_filereflist == NULL,
            "gli_filereflist is NULL after glk_fileref_destroy() of only fileref");
    }

    /* ---- glk_fileref_destroy with NULL ---- */

    {
        /* Should not crash */
        glk_fileref_destroy(NULL);
        TEST_ASSERT(1,
            "glk_fileref_destroy(NULL) does not crash");
    }

    /* ---- gli_delete_fileref with NULL ---- */

    {
        /* Should not crash */
        gli_delete_fileref(NULL);
        TEST_ASSERT(1,
            "gli_delete_fileref(NULL) does not crash");
    }

    /* ---- Dispatch registration callback on fileref creation ---- */

    {
        /* Save existing callbacks */
        gidispatch_rock_t (*saved_regi)(void *, glui32) = gli_register_obj;
        void (*saved_unregi)(void *, glui32, gidispatch_rock_t) = gli_unregister_obj;

        mock_reset();
        mock_regi_result.num = 42;

        gidispatch_set_object_registry(mock_register_obj, mock_unregister_obj);

        fileref_t *f = gli_new_fileref("dispatch.gls", 0, 0);
        TEST_ASSERT(f != NULL,
            "Created fileref for dispatch registration test");
        TEST_ASSERT(mock_regi_called == 1,
            "mock_register_obj called once on fileref creation");
        TEST_ASSERT(mock_regi_obj == f,
            "mock_register_obj received correct fileref pointer");
        TEST_ASSERT(mock_regi_class == gidisp_Class_Fileref,
            "mock_register_obj received gidisp_Class_Fileref");
        TEST_ASSERT(f->disprock.num == 42,
            "fileref disprock set to value returned by mock_register_obj");

        /* Clean up */
        gli_delete_fileref(f);

        /* Restore callbacks */
        gidispatch_set_object_registry(saved_regi, saved_unregi);
    }

    /* ---- Dispatch unregistration callback on fileref destruction ---- */

    {
        /* Save existing callbacks */
        gidispatch_rock_t (*saved_regi)(void *, glui32) = gli_register_obj;
        void (*saved_unregi)(void *, glui32, gidispatch_rock_t) = gli_unregister_obj;

        mock_reset();
        mock_regi_result.num = 99;

        gidispatch_set_object_registry(mock_register_obj, mock_unregister_obj);

        fileref_t *f = gli_new_fileref("undispatch.gls", 0, 0);
        TEST_ASSERT(f != NULL,
            "Created fileref for dispatch unregistration test");
        TEST_ASSERT(mock_regi_called == 1,
            "mock_register_obj called on fileref creation");

        /* Reset mock counters to track unregister separately */
        mock_regi_called = 0;
        mock_regi_obj = NULL;
        mock_regi_class = 0xFFFFFFFF;

        gli_delete_fileref(f);

        TEST_ASSERT(mock_unregi_called == 1,
            "mock_unregister_obj called once on fileref destruction");
        TEST_ASSERT(mock_unregi_obj == f,
            "mock_unregister_obj received correct fileref pointer");
        TEST_ASSERT(mock_unregi_class == gidisp_Class_Fileref,
            "mock_unregister_obj received gidisp_Class_Fileref");
        TEST_ASSERT(mock_unregi_rock.num == 99,
            "mock_unregister_obj received correct disprock (99)");

        /* Restore callbacks */
        gidispatch_set_object_registry(saved_regi, saved_unregi);
    }

    /* ---- Fileref creation with NULL callbacks succeeds ---- */

    {
        /* Save existing callbacks */
        gidispatch_rock_t (*saved_regi)(void *, glui32) = gli_register_obj;
        void (*saved_unregi)(void *, glui32, gidispatch_rock_t) = gli_unregister_obj;

        gidispatch_set_object_registry(NULL, NULL);

        fileref_t *f = gli_new_fileref("nocallback.gls", 0, 0);
        TEST_ASSERT(f != NULL,
            "gli_new_fileref() succeeds with NULL callbacks");
        TEST_ASSERT(f->disprock.num == 0,
            "fileref disprock is 0 when no callbacks registered");

        gli_delete_fileref(f);
        TEST_ASSERT(1,
            "gli_delete_fileref() with NULL callbacks does not crash");

        /* Restore callbacks */
        gidispatch_set_object_registry(saved_regi, saved_unregi);
    }

    /* ---- gidispatch_get_objrock returns correct disprock for filerefs ---- */

    {
        /* Save existing callbacks */
        gidispatch_rock_t (*saved_regi)(void *, glui32) = gli_register_obj;
        void (*saved_unregi)(void *, glui32, gidispatch_rock_t) = gli_unregister_obj;

        mock_reset();
        mock_regi_result.num = 789;

        gidispatch_set_object_registry(mock_register_obj, mock_unregister_obj);

        fileref_t *f = gli_new_fileref("rockcheck.gls", 0, 0);
        TEST_ASSERT(f != NULL,
            "Created fileref for get_objrock test");

        gidispatch_rock_t rock = gidispatch_get_objrock(f, gidisp_Class_Fileref);
        TEST_ASSERT(rock.num == 789,
            "gidispatch_get_objrock returns correct fileref disprock");

        gli_delete_fileref(f);

        /* Restore callbacks */
        gidispatch_set_object_registry(saved_regi, saved_unregi);
    }

    /* =====================================================================
     * Phase 3A.2 — Fileref Creation Tests
     * ===================================================================== */

    /* ---- glk_fileref_create_by_name: appends .glkdata suffix ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_Data, "mydata", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name(fileusage_Data, \"mydata\") succeeds");
        TEST_ASSERT(strstr(f->filename, "mydata.glkdata") != NULL,
            "filename contains \"mydata.glkdata\"");
        TEST_ASSERT(strcmp(f->filename, "mydata.glkdata") == 0,
            "filename is exactly \"mydata.glkdata\"");
        TEST_ASSERT(f->usage == fileusage_Data,
            "usage is fileusage_Data");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: appends .glksave suffix ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_SavedGame, "savegame", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name(fileusage_SavedGame) succeeds");
        TEST_ASSERT(strstr(f->filename, "savegame.glksave") != NULL,
            "filename contains \"savegame.glksave\"");
        TEST_ASSERT(strcmp(f->filename, "savegame.glksave") == 0,
            "filename is exactly \"savegame.glksave\"");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: appends .txt suffix ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_Transcript, "script", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name(fileusage_Transcript) succeeds");
        TEST_ASSERT(strstr(f->filename, "script.txt") != NULL,
            "filename contains \"script.txt\"");
        TEST_ASSERT(strcmp(f->filename, "script.txt") == 0,
            "filename is exactly \"script.txt\"");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: appends .glkrec suffix ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_InputRecord, "replay", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name(fileusage_InputRecord) succeeds");
        TEST_ASSERT(strstr(f->filename, "replay.glkrec") != NULL,
            "filename contains \"replay.glkrec\"");
        TEST_ASSERT(strcmp(f->filename, "replay.glkrec") == 0,
            "filename is exactly \"replay.glkrec\"");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: no double-extension (.glkdata) ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_Data, "config.glkdata", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name() with .glkdata extension succeeds");
        TEST_ASSERT(strstr(f->filename, "config.glkdata") != NULL,
            "filename contains \"config.glkdata\"");
        TEST_ASSERT(strstr(f->filename, ".glkdata.glkdata") == NULL,
            "filename does NOT have double .glkdata suffix");
        TEST_ASSERT(strcmp(f->filename, "config.glkdata") == 0,
            "filename is exactly \"config.glkdata\" (no double-append)");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: no double-extension (.glksave) ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_SavedGame, "mysave.glksave", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name() with .glksave extension succeeds");
        TEST_ASSERT(strstr(f->filename, ".glksave.glksave") == NULL,
            "filename does NOT have double .glksave suffix");
        TEST_ASSERT(strcmp(f->filename, "mysave.glksave") == 0,
            "filename is exactly \"mysave.glksave\" (no double-append)");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: no double-extension (.txt) ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_Transcript, "log.txt", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name() with .txt extension succeeds");
        TEST_ASSERT(strstr(f->filename, ".txt.txt") == NULL,
            "filename does NOT have double .txt suffix");
        TEST_ASSERT(strcmp(f->filename, "log.txt") == 0,
            "filename is exactly \"log.txt\" (no double-append)");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: no double-extension (.glkrec) ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_InputRecord, "input.glkrec", 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name() with .glkrec extension succeeds");
        TEST_ASSERT(strstr(f->filename, ".glkrec.glkrec") == NULL,
            "filename does NOT have double .glkrec suffix");
        TEST_ASSERT(strcmp(f->filename, "input.glkrec") == 0,
            "filename is exactly \"input.glkrec\" (no double-append)");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: rock value stored ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_Data, "rocktest", 42);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_name() with rock succeeds");
        TEST_ASSERT(glk_fileref_get_rock(f) == 42,
            "glk_fileref_get_rock returns 42");
        TEST_ASSERT(f->rock == 42,
            "fileref rock field is 42");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_name: NULL name returns NULL ---- */

    {
        fileref_t *f = glk_fileref_create_by_name(
            fileusage_Data, NULL, 0);
        TEST_ASSERT(f == NULL,
            "glk_fileref_create_by_name(NULL) returns NULL");
    }

    /* ---- glk_fileref_create_by_prompt: fixed filename for SavedGame ---- */

    {
        fileref_t *f = glk_fileref_create_by_prompt(
            fileusage_SavedGame, 0x01, 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_prompt(fileusage_SavedGame) succeeds");
        TEST_ASSERT(strcmp(f->filename, "nextgit.sav") == 0,
            "filename is exactly \"nextgit.sav\"");
        TEST_ASSERT(f->usage == fileusage_SavedGame,
            "usage is fileusage_SavedGame");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_prompt: fixed filename for Transcript ---- */

    {
        fileref_t *f = glk_fileref_create_by_prompt(
            fileusage_Transcript, 0x01, 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_prompt(fileusage_Transcript) succeeds");
        TEST_ASSERT(strcmp(f->filename, "transcript.txt") == 0,
            "filename is exactly \"transcript.txt\"");
        TEST_ASSERT(f->usage == fileusage_Transcript,
            "usage is fileusage_Transcript");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_prompt: fixed filename for InputRecord ---- */

    {
        fileref_t *f = glk_fileref_create_by_prompt(
            fileusage_InputRecord, 0x01, 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_prompt(fileusage_InputRecord) succeeds");
        TEST_ASSERT(strcmp(f->filename, "input.rec") == 0,
            "filename is exactly \"input.rec\"");
        TEST_ASSERT(f->usage == fileusage_InputRecord,
            "usage is fileusage_InputRecord");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_prompt: fixed filename for Data ---- */

    {
        fileref_t *f = glk_fileref_create_by_prompt(
            fileusage_Data, 0x01, 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_prompt(fileusage_Data) succeeds");
        TEST_ASSERT(strcmp(f->filename, "data.glkdata") == 0,
            "filename is exactly \"data.glkdata\"");
        TEST_ASSERT(f->usage == fileusage_Data,
            "usage is fileusage_Data");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_by_prompt: rock value stored ---- */

    {
        fileref_t *f = glk_fileref_create_by_prompt(
            fileusage_SavedGame, 0x01, 99);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_by_prompt() with rock succeeds");
        TEST_ASSERT(glk_fileref_get_rock(f) == 99,
            "glk_fileref_get_rock returns 99");
        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_temp: creates unique temp file ---- */

    {
        fileref_t *f = glk_fileref_create_temp(
            fileusage_Data, 0);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_temp() succeeds");
        TEST_ASSERT(f->filename != NULL,
            "temp fileref has a filename");
        TEST_ASSERT(strncmp(f->filename, "nextgit-", 8) == 0,
            "temp filename starts with \"nextgit-\"");
        TEST_ASSERT(f->usage == fileusage_Data,
            "temp fileref has correct usage");

        /* Clean up: delete the temp file and destroy the fileref.
         * We use unlink() directly since glk_fileref_delete_file is
         * still a stub in Phase 3A.2. */
        if (f->filename)
            unlink(f->filename);

        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_temp: two calls produce different names ---- */

    {
        fileref_t *f1 = glk_fileref_create_temp(
            fileusage_Data, 0);
        fileref_t *f2 = glk_fileref_create_temp(
            fileusage_Data, 0);
        TEST_ASSERT(f1 != NULL,
            "glk_fileref_create_temp() first call succeeds");
        TEST_ASSERT(f2 != NULL,
            "glk_fileref_create_temp() second call succeeds");
        TEST_ASSERT(strcmp(f1->filename, f2->filename) != 0,
            "two temp filerefs have different filenames");

        /* Clean up */
        if (f1->filename)
            unlink(f1->filename);
        if (f2->filename)
            unlink(f2->filename);

        glk_fileref_destroy(f1);
        glk_fileref_destroy(f2);
    }

    /* ---- glk_fileref_create_temp: rock value stored ---- */

    {
        fileref_t *f = glk_fileref_create_temp(
            fileusage_SavedGame, 55);
        TEST_ASSERT(f != NULL,
            "glk_fileref_create_temp() with rock succeeds");
        TEST_ASSERT(glk_fileref_get_rock(f) == 55,
            "glk_fileref_get_rock returns 55");

        if (f->filename)
            unlink(f->filename);

        glk_fileref_destroy(f);
    }

    /* ---- glk_fileref_create_from_fileref: clones filename ---- */

    {
        fileref_t *orig = glk_fileref_create_by_name(
            fileusage_Data, "source", 0);
        TEST_ASSERT(orig != NULL,
            "Created original fileref for create_from_fileref test");

        fileref_t *clone = glk_fileref_create_from_fileref(
            fileusage_Transcript, orig, 0);
        TEST_ASSERT(clone != NULL,
            "glk_fileref_create_from_fileref() succeeds");
        TEST_ASSERT(clone != orig,
            "clone is a different pointer from original");
        TEST_ASSERT(strcmp(clone->filename, orig->filename) == 0,
            "clone has same filename as original");
        TEST_ASSERT(strcmp(clone->filename, "source.glkdata") == 0,
            "clone filename is \"source.glkdata\"");
        TEST_ASSERT(clone->usage == fileusage_Transcript,
            "clone usage is fileusage_Transcript (not Data)");

        glk_fileref_destroy(clone);
        glk_fileref_destroy(orig);
    }

    /* ---- glk_fileref_create_from_fileref: different usage and rock ---- */

    {
        fileref_t *orig = glk_fileref_create_by_name(
            fileusage_SavedGame, "game", 10);
        TEST_ASSERT(orig != NULL,
            "Created original fileref for create_from_fileref usage test");

        fileref_t *clone = glk_fileref_create_from_fileref(
            fileusage_Data, orig, 20);
        TEST_ASSERT(clone != NULL,
            "glk_fileref_create_from_fileref() with different usage succeeds");
        TEST_ASSERT(clone->usage == fileusage_Data,
            "clone usage is fileusage_Data");
        TEST_ASSERT(glk_fileref_get_rock(clone) == 20,
            "clone rock is 20");

        /* Original should be unchanged */
        TEST_ASSERT(orig->usage == fileusage_SavedGame,
            "original usage is still fileusage_SavedGame");
        TEST_ASSERT(glk_fileref_get_rock(orig) == 10,
            "original rock is still 10");

        glk_fileref_destroy(clone);
        glk_fileref_destroy(orig);
    }

    /* ---- glk_fileref_create_from_fileref: NULL fref returns NULL ---- */

    {
        fileref_t *f = glk_fileref_create_from_fileref(
            fileusage_Data, NULL, 0);
        TEST_ASSERT(f == NULL,
            "glk_fileref_create_from_fileref(NULL) returns NULL");
    }

    /* ---- Iteration: all creation types appear in list ---- */

    {
        fileref_t *f_by_name = glk_fileref_create_by_name(
            fileusage_Data, "iter1", 0);
        fileref_t *f_by_prompt = glk_fileref_create_by_prompt(
            fileusage_SavedGame, 0x01, 0);
        fileref_t *f_temp = glk_fileref_create_temp(
            fileusage_Transcript, 0);

        TEST_ASSERT(f_by_name != NULL,
            "create_by_name for iteration test succeeds");
        TEST_ASSERT(f_by_prompt != NULL,
            "create_by_prompt for iteration test succeeds");
        TEST_ASSERT(f_temp != NULL,
            "create_temp for iteration test succeeds");

        /* Iterate through all filerefs and count them */
        int count = 0;
        fileref_t *iter = glk_fileref_iterate(NULL, NULL);
        while (iter != NULL) {
            count++;
            iter = glk_fileref_iterate(iter, NULL);
        }
        TEST_ASSERT(count >= 3,
            "iteration sees at least 3 filerefs after all creation types");

        /* Clean up temp files */
        if (f_temp->filename)
            unlink(f_temp->filename);

        glk_fileref_destroy(f_temp);
        glk_fileref_destroy(f_by_prompt);
        glk_fileref_destroy(f_by_name);

    /* Verify list is empty after destroying all */
        iter = glk_fileref_iterate(NULL, NULL);
        TEST_ASSERT(iter == NULL,
            "iteration returns NULL after all filerefs destroyed");
    }

    /* =====================================================================
     * Phase 3A.3 — Platform File Write I/O Tests
     * ===================================================================== */

#define TEST_FILE "/tmp/nextgit-test-3a3-write.bin"
#define TEST_FILE_APPEND "/tmp/nextgit-test-3a3-append.bin"

    /* ---- open_write success ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "nextglk_file_open_write() creates a NextGlkFile");
        nextglk_file_close(f);
        unlink(TEST_FILE);
    }

    /* ---- open_write invalid path ---- */

    {
        NextGlkFile *f = nextglk_file_open_write("/nonexistent_dir_xyz/test.bin");
        TEST_ASSERT(f == NULL,
            "nextglk_file_open_write() with invalid path returns NULL");
    }

    /* ---- open_write with NULL path ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(NULL);
        TEST_ASSERT(f == NULL,
            "nextglk_file_open_write(NULL) returns NULL");
    }

    /* ---- write small buffer ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "open_write for small buffer test succeeds");

        const char *data = "Hello, NextGit!";
        uint32_t written = nextglk_file_write(f, data, strlen(data));
        TEST_ASSERT(written == strlen(data),
            "nextglk_file_write() writes correct number of bytes");

        nextglk_file_close(f);

        /* Verify file contents */
        FILE *fp = fopen(TEST_FILE, "rb");
        TEST_ASSERT(fp != NULL,
            "can open test file for reading");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == strlen(data),
            "file contains correct number of bytes");
        TEST_ASSERT(strcmp(buf, data) == 0,
            "file content matches written data");
        fclose(fp);
        unlink(TEST_FILE);
    }

    /* ---- write empty buffer ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "open_write for empty buffer test succeeds");

        uint32_t written = nextglk_file_write(f, "data", 0);
        TEST_ASSERT(written == 0,
            "nextglk_file_write() with zero length returns 0");

        nextglk_file_close(f);

        /* File should exist but be empty */
        FILE *fp = fopen(TEST_FILE, "rb");
        TEST_ASSERT(fp != NULL,
            "can open test file for reading");
        long file_size;
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        TEST_ASSERT(file_size == 0,
            "file is empty after writing zero bytes");
        fclose(fp);
        unlink(TEST_FILE);
    }

    /* ---- write multiple buffers ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "open_write for multiple buffers test succeeds");

        uint32_t w1 = nextglk_file_write(f, "ABC", 3);
        TEST_ASSERT(w1 == 3,
            "first write returns 3");

        uint32_t w2 = nextglk_file_write(f, "DEFG", 4);
        TEST_ASSERT(w2 == 4,
            "second write returns 4");

        uint32_t w3 = nextglk_file_write(f, "HI", 2);
        TEST_ASSERT(w3 == 2,
            "third write returns 2");

        nextglk_file_close(f);

        /* Verify concatenated content */
        FILE *fp = fopen(TEST_FILE, "rb");
        TEST_ASSERT(fp != NULL,
            "can open test file for reading");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 9,
            "file contains 9 bytes");
        TEST_ASSERT(strcmp(buf, "ABCDEFGHI") == 0,
            "file content is concatenated writes");
        fclose(fp);
        unlink(TEST_FILE);
    }

    /* ---- append existing file ---- */

    {
        /* First, write initial content */
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE_APPEND);
        TEST_ASSERT(f != NULL,
            "open_write for append test succeeds");
        nextglk_file_write(f, "first\n", 6);
        nextglk_file_close(f);

        /* Now append */
        f = nextglk_file_append(TEST_FILE_APPEND);
        TEST_ASSERT(f != NULL,
            "nextglk_file_append() opens existing file");
        nextglk_file_write(f, "second\n", 7);
        nextglk_file_close(f);

        /* Verify both lines present */
        FILE *fp = fopen(TEST_FILE_APPEND, "rb");
        TEST_ASSERT(fp != NULL,
            "can open appended file for reading");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 13,
            "appended file has 13 bytes");
        TEST_ASSERT(strcmp(buf, "first\nsecond\n") == 0,
            "appended file contains both lines");
        fclose(fp);
        unlink(TEST_FILE_APPEND);
    }

    /* ---- append missing file (creates it) ---- */

    {
        /* Ensure file does not exist */
        unlink(TEST_FILE_APPEND);

        NextGlkFile *f = nextglk_file_append(TEST_FILE_APPEND);
        TEST_ASSERT(f != NULL,
            "nextglk_file_append() creates missing file");
        nextglk_file_write(f, "newfile\n", 8);
        nextglk_file_close(f);

        /* Verify file was created with content */
        FILE *fp = fopen(TEST_FILE_APPEND, "rb");
        TEST_ASSERT(fp != NULL,
            "appended (missing) file was created");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 8,
            "file contains 8 bytes");
        TEST_ASSERT(strcmp(buf, "newfile\n") == 0,
            "file content matches");
        fclose(fp);
        unlink(TEST_FILE_APPEND);
    }

    /* ---- append with NULL path ---- */

    {
        NextGlkFile *f = nextglk_file_append(NULL);
        TEST_ASSERT(f == NULL,
            "nextglk_file_append(NULL) returns NULL");
    }

    /* ---- append with invalid path ---- */

    {
        NextGlkFile *f = nextglk_file_append("/nonexistent_dir_xyz/test.bin");
        TEST_ASSERT(f == NULL,
            "nextglk_file_append() with invalid path returns NULL");
    }

    /* ---- close file ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "open_write for close test succeeds");
        nextglk_file_close(f);
        TEST_ASSERT(1,
            "nextglk_file_close() on valid file succeeds");
        unlink(TEST_FILE);
    }

    /* ---- close NULL ---- */

    {
        nextglk_file_close(NULL);
        TEST_ASSERT(1,
            "nextglk_file_close(NULL) does not crash");
    }

    /* ---- write with NULL file ---- */

    {
        uint32_t written = nextglk_file_write(NULL, "data", 4);
        TEST_ASSERT(written == 0,
            "nextglk_file_write(NULL, ...) returns 0");
    }

    /* ---- write with NULL buffer ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "open_write for NULL buffer write test succeeds");

        uint32_t written = nextglk_file_write(f, NULL, 4);
        TEST_ASSERT(written == 0,
            "nextglk_file_write(f, NULL, ...) returns 0");

        nextglk_file_close(f);
        unlink(TEST_FILE);
    }

    /* ---- open_write truncates existing file ---- */

    {
        /* Write initial file with content */
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "first open_write succeeds");
        nextglk_file_write(f, "original_content_long", 21);
        nextglk_file_close(f);

        /* Re-open with write (should truncate) */
        f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "second open_write succeeds (truncates)");
        nextglk_file_write(f, "short", 5);
        nextglk_file_close(f);

        /* Verify file is truncated and contains only new content */
        FILE *fp = fopen(TEST_FILE, "rb");
        TEST_ASSERT(fp != NULL,
            "can open truncated file for reading");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 5,
            "file has 5 bytes (truncated)");
        TEST_ASSERT(strcmp(buf, "short") == 0,
            "file content is only the new data");
        fclose(fp);
        unlink(TEST_FILE);
    }

    /* ---- write byte value 0 ---- */

    {
        NextGlkFile *f = nextglk_file_open_write(TEST_FILE);
        TEST_ASSERT(f != NULL,
            "open_write for binary zero test succeeds");

        /* Write a buffer containing embedded null bytes */
        const unsigned char binary[] = { 'A', 0, 'B', 0, 'C' };
        uint32_t written = nextglk_file_write(f, binary, 5);
        TEST_ASSERT(written == 5,
            "write binary data with null bytes returns 5");

        nextglk_file_close(f);

        /* Verify binary content */
        FILE *fp = fopen(TEST_FILE, "rb");
        TEST_ASSERT(fp != NULL,
            "can open binary file for reading");
        unsigned char buf[8] = {0};
        size_t n = fread(buf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "read 5 bytes back");
        TEST_ASSERT(buf[0] == 'A' && buf[1] == 0 && buf[2] == 'B'
            && buf[3] == 0 && buf[4] == 'C',
            "binary content preserved including null bytes");
        fclose(fp);
        unlink(TEST_FILE);
    }

#undef TEST_FILE
#undef TEST_FILE_APPEND

    /* =====================================================================
     * Phase 3A.4 — File Stream Open and Close Tests
     * ===================================================================== */

#define TEST_3A4_WRITE  "/tmp/nextgit-test-3a4-write.bin"
#define TEST_3A4_APPEND "/tmp/nextgit-test-3a4-append.bin"

    /* ---- open file stream write mode ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4write", 0);
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
        unlink("test3a4write.glkdata");
    }

    /* ---- open file stream append mode ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Transcript, "test3a4append", 0);
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

        /* In append mode, the file should exist on disk.
         * Use access() directly since glk_fileref_does_file_exist is
         * still a stub (deferred to Phase 3A.7). */
        TEST_ASSERT(access("test3a4append.txt", F_OK) == 0,
            "3A.4: append stream creates file on disk");

        glk_fileref_destroy(fref);
        unlink("test3a4append.txt");
    }

    /* ---- NULL fileref returns NULL ---- */

    {
        strid_t str = glk_stream_open_file(NULL, filemode_Write, 0);
        TEST_ASSERT(str == NULL,
            "3A.4: glk_stream_open_file(NULL, ...) returns NULL");
    }

    /* ---- read mode returns NULL ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4read", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for read-mode test");

        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        TEST_ASSERT(str == NULL,
            "3A.4: glk_stream_open_file(filemode_Read) returns NULL (deferred to Phase 3B)");

        glk_fileref_destroy(fref);
    }

    /* ---- stream registered in gli_streamlist ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4list", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for stream list test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for list test");

        /* Search gli_streamlist for the stream */
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
        unlink("test3a4list.glkdata");
    }

    /* ---- stream removed from list on close ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4removed", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for stream removal test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for removal test");

        glk_stream_close(str, NULL);

        /* Verify stream is no longer in gli_streamlist */
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
        unlink("test3a4removed.glkdata");
    }

    /* ---- close file stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4close", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for close test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for close test");

        glk_stream_close(str, NULL);
        TEST_ASSERT(1,
            "3A.4: glk_stream_close() on file stream does not crash");

        glk_fileref_destroy(fref);
        unlink("test3a4close.glkdata");
    }

    /* ---- close current stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4current", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for current stream test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream for current stream test");

        /* Set as current stream */
        glk_stream_set_current(str);
        TEST_ASSERT(glk_stream_get_current() == str,
            "3A.4: stream is current after glk_stream_set_current");

        /* Close the current stream */
        glk_stream_close(str, NULL);
        TEST_ASSERT(glk_stream_get_current() == NULL,
            "3A.4: current stream is NULL after closing current stream");

        glk_fileref_destroy(fref);
        unlink("test3a4current.glkdata");
    }

    /* ---- stream rock preserved (gaps documented) ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a4rock", 0);
        TEST_ASSERT(fref != NULL,
            "3A.4: created fileref for rock test");

        /* Open with rock=42 */
        strid_t str = glk_stream_open_file(fref, filemode_Write, 42);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream with rock=42");

        /* glk_stream_get_rock currently returns 0 (rock storage is a known gap) */
        TEST_ASSERT(glk_stream_get_rock(str) == 0,
            "3A.4: glk_stream_get_rock returns 0 (rock storage deferred)");

        glk_stream_close(str, NULL);

        /* Open with rock=0 */
        str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.4: opened file stream with rock=0");
        TEST_ASSERT(glk_stream_get_rock(str) == 0,
            "3A.4: glk_stream_get_rock returns 0 for rock=0 stream");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("test3a4rock.glkdata");
    }

#undef TEST_3A4_WRITE
#undef TEST_3A4_APPEND

    /* =====================================================================
     * Phase 3A.5 — File Stream Output Tests
     * ===================================================================== */

#define TEST_3A5_CHAR    "/tmp/nextgit-test-3a5-char.bin"
#define TEST_3A5_BUFFER  "/tmp/nextgit-test-3a5-buffer.bin"
#define TEST_3A5_STRING  "/tmp/nextgit-test-3a5-string.bin"
#define TEST_3A5_MULTI   "/tmp/nextgit-test-3a5-multi.bin"
#define TEST_3A5_EMPTY   "/tmp/nextgit-test-3a5-empty.bin"
#define TEST_3A5_COUNT   "/tmp/nextgit-test-3a5-count.bin"
#define TEST_3A5_APPEND  "/tmp/nextgit-test-3a5-append.bin"
#define TEST_3A5_BINARY  "/tmp/nextgit-test-3a5-binary.bin"

    /* ---- file stream single character output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5char", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for single char output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for single char output");

        glk_put_char_stream(str, 'X');

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        /* Verify file contents */
        FILE *fp = fopen("test3a5char.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after single char write");
        int c = fgetc(fp);
        TEST_ASSERT(c == 'X',
            "3A.5: file contains 'X'");
        c = fgetc(fp);
        TEST_ASSERT(c == EOF,
            "3A.5: file contains exactly one byte");
        fclose(fp);
        unlink("test3a5char.glkdata");
    }

    /* ---- file stream multiple character output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5multichar", 0);
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

        /* Verify file contents */
        FILE *fp = fopen("test3a5multichar.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after multiple char writes");
        char buf[8] = {0};
        size_t n = fread(buf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "3A.5: file contains 5 bytes");
        TEST_ASSERT(strcmp(buf, "Hello") == 0,
            "3A.5: file contents match \"Hello\"");
        fclose(fp);
        unlink("test3a5multichar.glkdata");
    }

    /* ---- file stream buffer output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5buf", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for buffer output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for buffer output");

        const char *data = "Hello, NextGit World!";
        glk_put_buffer_stream(str, (char *)data, strlen(data));

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        /* Verify file contents */
        FILE *fp = fopen("test3a5buf.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after buffer write");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == strlen(data),
            "3A.5: file contains correct number of bytes");
        TEST_ASSERT(strcmp(buf, data) == 0,
            "3A.5: file contents match written buffer");
        fclose(fp);
        unlink("test3a5buf.glkdata");
    }

    /* ---- file stream string output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5str", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for string output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for string output");

        glk_put_string_stream(str, "String via put_string_stream");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        /* Verify file contents */
        FILE *fp = fopen("test3a5str.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after string write");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == strlen("String via put_string_stream"),
            "3A.5: file contains correct number of bytes");
        TEST_ASSERT(strcmp(buf, "String via put_string_stream") == 0,
            "3A.5: file contents match written string");
        fclose(fp);
        unlink("test3a5str.glkdata");
    }

    /* ---- writecount updates correctly ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5count", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for writecount test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for writecount test");

        stream_result_t result;
        memset(&result, 0, sizeof(result));

        /* Write 4 characters individually */
        glk_put_char_stream(str, 'A');
        glk_put_char_stream(str, 'B');
        glk_put_char_stream(str, 'C');
        glk_put_char_stream(str, 'D');

        /* Write 5 bytes via buffer */
        glk_put_buffer_stream(str, "EFGHI", 5);

        /* Write 6 bytes via string (null terminator not written) */
        glk_put_string_stream(str, "JKLMNO");

        glk_stream_close(str, &result);

        TEST_ASSERT(result.writecount == 15,
            "3A.5: writecount is 15 (4 chars + 5 buffer + 6 string)");
        TEST_ASSERT(result.readcount == 0,
            "3A.5: readcount is 0 (write-only stream)");

        glk_fileref_destroy(fref);

        /* Verify file contents: ABCDEFGHIJKLMNO = 15 bytes */
        FILE *fp = fopen("test3a5count.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after writecount test");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 15,
            "3A.5: file contains 15 bytes");
        TEST_ASSERT(strcmp(buf, "ABCDEFGHIJKLMNO") == 0,
            "3A.5: file contents match expected concatenation");
        fclose(fp);
        unlink("test3a5count.glkdata");
    }

    /* ---- empty buffer output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5empty", 0);
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

        /* Verify file is empty */
        FILE *fp = fopen("test3a5empty.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: file exists after empty buffer write");
        long file_size;
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        TEST_ASSERT(file_size == 0,
            "3A.5: file is empty (0 bytes)");
        fclose(fp);
        unlink("test3a5empty.glkdata");
    }

    /* ---- multiple writes to the same stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5multi", 0);
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

        /* Verify: [chunk1|chunk2] */
        FILE *fp = fopen("test3a5multi.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open file for reading after multiple writes");
        char buf[32] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 15,
            "3A.5: file contains 15 bytes");
        TEST_ASSERT(strcmp(buf, "[chunk1|chunk2]") == 0,
            "3A.5: file contents match expected pattern");
        fclose(fp);
        unlink("test3a5multi.glkdata");
    }

    /* ---- append stream output ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Transcript, "test3a5appendout", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for append output test");

        /* First write session: create and write initial data */
        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream (write mode) for append test");
        glk_put_string_stream(str, "Line 1\n");
        glk_stream_close(str, NULL);

        /* Second session: open for append */
        str = glk_stream_open_file(fref, filemode_WriteAppend, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: reopened file stream (append mode) for append test");
        glk_put_string_stream(str, "Line 2\n");
        glk_stream_close(str, NULL);

        /* Third session: append more */
        str = glk_stream_open_file(fref, filemode_WriteAppend, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: reopened file stream (append mode) for second append");
        glk_put_string_stream(str, "Line 3\n");
        glk_stream_close(str, NULL);

        glk_fileref_destroy(fref);

        /* Verify all three lines are in the file */
        FILE *fp = fopen("test3a5appendout.txt", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open appended file for reading");
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        TEST_ASSERT(n == 21,
            "3A.5: appended file contains 21 bytes");
        TEST_ASSERT(strcmp(buf, "Line 1\nLine 2\nLine 3\n") == 0,
            "3A.5: appended file contains all three lines");
        fclose(fp);
        unlink("test3a5appendout.txt");
    }

    /* ---- binary data write via glk_put_buffer_stream ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a5binary", 0);
        TEST_ASSERT(fref != NULL,
            "3A.5: created fileref for binary output test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.5: opened file stream for binary output");

        const unsigned char bin[] = { 0x00, 0xFF, 'A', 0x7F, 0x80 };
        glk_put_buffer_stream(str, (const char *)bin, 5);

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        /* Verify binary content */
        FILE *fp = fopen("test3a5binary.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.5: can open binary file for reading");
        unsigned char buf[8] = {0};
        size_t n = fread(buf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "3A.5: binary file contains 5 bytes");
        TEST_ASSERT(buf[0] == 0x00 && buf[1] == 0xFF && buf[2] == 'A'
            && buf[3] == 0x7F && buf[4] == 0x80,
            "3A.5: binary data preserved correctly including 0x00 and 0xFF");
        fclose(fp);
        unlink("test3a5binary.glkdata");
    }

#undef TEST_3A5_CHAR
#undef TEST_3A5_BUFFER
#undef TEST_3A5_STRING
#undef TEST_3A5_MULTI
#undef TEST_3A5_EMPTY
#undef TEST_3A5_COUNT
#undef TEST_3A5_APPEND
#undef TEST_3A5_BINARY

    /* =====================================================================
     * Phase 3A.6 — File Stream Position Tests
     * ===================================================================== */

#define TEST_3A6_POS       "/tmp/nextgit-test-3a6-pos.bin"
#define TEST_3A6_RELATIVE  "/tmp/nextgit-test-3a6-relative.bin"
#define TEST_3A6_END       "/tmp/nextgit-test-3a6-end.bin"
#define TEST_3A6_OVERWRITE "/tmp/nextgit-test-3a6-overwrite.bin"
#define TEST_3A6_MULTI     "/tmp/nextgit-test-3a6-multi.bin"
#define TEST_3A6_PERSIST   "/tmp/nextgit-test-3a6-persist.bin"

    /* ---- get position on empty file ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6empty", 0);
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
        unlink("test3a6empty.glkdata");
    }

    /* ---- position after writes ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6poswrite", 0);
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
        unlink("test3a6poswrite.glkdata");
    }

    /* ---- seek to start ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6seekstart", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for seek-to-start test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for seek-to-start test");

        /* Write some data to move past position 0 */
        glk_put_string_stream(str, "abcdefgh");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 8,
            "3A.6: position is 8 after writing 8 bytes");

        /* Seek to start */
        glk_stream_set_position(str, 0, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 after seekmode_Start to 0");

        /* Seek to a positive offset from start */
        glk_stream_set_position(str, 4, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 4,
            "3A.6: position is 4 after seekmode_Start to 4");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("test3a6seekstart.glkdata");
    }

    /* ---- seek relative to current ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6seekcur", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for seek-relative test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for seek-relative test");

        /* Write "Hello" to get to position 5 */
        glk_put_string_stream(str, "Hello");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after writing \"Hello\"");

        /* Seek backward 2 bytes relative to current */
        glk_stream_set_position(str, -2, seekmode_Current);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 3,
            "3A.6: position is 3 after seekmode_Current to -2");

        /* Seek forward 4 bytes relative to current */
        glk_stream_set_position(str, 4, seekmode_Current);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 7,
            "3A.6: position is 7 after seekmode_Current to +4");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("test3a6seekcur.glkdata");
    }

    /* ---- seek relative to end ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6seekend", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for seek-from-end test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for seek-from-end test");

        /* Write 10 bytes */
        glk_put_string_stream(str, "0123456789");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after writing 10 bytes");

        /* Seek 3 bytes back from end */
        glk_stream_set_position(str, -3, seekmode_End);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 7,
            "3A.6: position is 7 after seekmode_End to -3");

        /* Seek to end (offset 0 from end) */
        glk_stream_set_position(str, 0, seekmode_End);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after seekmode_End to 0");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);
        unlink("test3a6seekend.glkdata");
    }

    /* ---- overwrite existing bytes after seek ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6overwrite", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for overwrite-after-seek test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for overwrite-after-seek test");

        /* Write initial content */
        glk_put_string_stream(str, "Hello");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after writing \"Hello\"");

        /* Seek back to start and overwrite */
        glk_stream_set_position(str, 0, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 0,
            "3A.6: position is 0 after seek to start");

        /* Overwrite first 3 bytes with "XYZ" */
        glk_put_string_stream(str, "XYZ");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 3,
            "3A.6: position is 3 after writing \"XYZ\"");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        /* Verify file contents: "XYZlo" */
        FILE *fp = fopen("test3a6overwrite.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.6: can open file for reading after overwrite");
        char buf[16] = {0};
        size_t n = fread(buf, 1, 5, fp);
        TEST_ASSERT(n == 5,
            "3A.6: file contains 5 bytes after overwrite");
        TEST_ASSERT(strcmp(buf, "XYZlo") == 0,
            "3A.6: file content is \"XYZlo\" after overwriting first 3 bytes of \"Hello\"");
        fclose(fp);
        unlink("test3a6overwrite.glkdata");
    }

    /* ---- multiple seek operations ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6multiseek", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for multi-seek test");

        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream for multi-seek test");

        /* Write "AAAAABBBBBCCCCC" (15 bytes) */
        glk_put_string_stream(str, "AAAAABBBBBCCCCC");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 15,
            "3A.6: position is 15 after writing 15 bytes");

        /* Seek to position 5 and overwrite "BBBBB" with "XXXXX" */
        glk_stream_set_position(str, 5, seekmode_Start);
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 5,
            "3A.6: position is 5 after seek to 5");
        glk_put_string_stream(str, "XXXXX");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 10,
            "3A.6: position is 10 after writing \"XXXXX\"");

        /* Seek to position 10 and overwrite "CCCCC" with "YYYYY" */
        glk_stream_set_position(str, 10, seekmode_Start);
        glk_put_string_stream(str, "YYYYY");
        pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 15,
            "3A.6: position is 15 after writing \"YYYYY\"");

        /* Seek to position 0 and overwrite "AAAAA" with "ZZZZZ" */
        glk_stream_set_position(str, 0, seekmode_Start);
        glk_put_string_stream(str, "ZZZZZ");

        glk_stream_close(str, NULL);
        glk_fileref_destroy(fref);

        /* Verify file: "ZZZZZXXXXXYYYYY" */
        FILE *fp = fopen("test3a6multiseek.glkdata", "rb");
        TEST_ASSERT(fp != NULL,
            "3A.6: can open file for reading after multi-seek");
        char buf[32] = {0};
        size_t n = fread(buf, 1, 15, fp);
        TEST_ASSERT(n == 15,
            "3A.6: file contains 15 bytes after multi-seek");
        TEST_ASSERT(strcmp(buf, "ZZZZZXXXXXYYYYY") == 0,
            "3A.6: file content is \"ZZZZZXXXXXYYYYY\" after multi-seek overwrites");
        fclose(fp);
        unlink("test3a6multiseek.glkdata");
    }

    /* ---- invalid stream safety (NULL stream) ---- */

    {
        /* glk_stream_get_position(NULL) should return 0 */
        glui32 pos = glk_stream_get_position(NULL);
        TEST_ASSERT(pos == 0,
            "3A.6: glk_stream_get_position(NULL) returns 0");

        /* glk_stream_set_position(NULL, ...) should not crash */
        glk_stream_set_position(NULL, 0, seekmode_Start);
        TEST_ASSERT(1,
            "3A.6: glk_stream_set_position(NULL, ...) does not crash");
    }

    /* ---- file position persistence -- reopened file starts at 0 ---- */

    {
        frefid_t fref = glk_fileref_create_by_name(
            fileusage_Data, "test3a6persist", 0);
        TEST_ASSERT(fref != NULL,
            "3A.6: created fileref for position persistence test");

        /* First session: write data */
        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        TEST_ASSERT(str != NULL,
            "3A.6: opened file stream (first session)");

        glk_put_string_stream(str, "Session One Data");
        glui32 pos = glk_stream_get_position(str);
        TEST_ASSERT(pos == 16,
            "3A.6: position is 16 after writing 16 bytes in first session");

        glk_stream_close(str, NULL);

        /* Reopen the same file (truncates with filemode_Write) */
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
        unlink("test3a6persist.glkdata");
    }

#undef TEST_3A6_POS
#undef TEST_3A6_RELATIVE
#undef TEST_3A6_END
#undef TEST_3A6_OVERWRITE
#undef TEST_3A6_MULTI
#undef TEST_3A6_PERSIST

    return 0;
}
