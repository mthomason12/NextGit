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

#include "../nextglk/nextglk_internal.h"
#include "test_common.h"
#include <string.h>
#include <unistd.h>

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

    return 0;
}