/* dispatch_tests.c — Dispatch-layer registration tests for Commit 3
 *
 * Tests:
 *   - Stream creation with NULL callback pointers succeeds
 *   - Window creation with NULL callback pointers succeeds
 *   - Stream destruction with NULL callback pointers succeeds
 *   - Window destruction with NULL callback pointers succeeds
 *   - Object registration callback is invoked on stream creation
 *   - Object unregistration callback is invoked on stream destruction
 *   - Object registration callback is invoked on window creation
 *   - Object unregistration callback is invoked on window destruction
 *   - gidispatch_set_object_registry stores callbacks correctly
 *   - gidispatch_get_objrock returns correct disprock for streams and windows
 */

#include "../nextglk/nextglk_internal.h"
#include "test_common.h"
#include <string.h>

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

/* -------------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------------- */

int dispatch_tests_run(void)
{
    /* ---- Initial state: callbacks are NULL ---- */

    TEST_ASSERT(gli_register_obj == NULL,
        "gli_register_obj is NULL initially");
    TEST_ASSERT(gli_unregister_obj == NULL,
        "gli_unregister_obj is NULL initially");

    /* ---- Stream creation with NULL callbacks succeeds ---- */

    {
        stream_t *s = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(s != NULL,
            "gli_new_stream() succeeds with NULL callbacks");
        TEST_ASSERT(s->disprock.num == 0,
            "stream disprock is 0 when no callbacks registered");
        gli_delete_stream(s, NULL);
    }

    /* ---- Window creation with NULL callbacks succeeds ---- */

    {
        window_t *w = gli_new_window(0);
        TEST_ASSERT(w != NULL,
            "gli_new_window() succeeds with NULL callbacks");
        TEST_ASSERT(w->disprock.num == 0,
            "window disprock is 0 when no callbacks registered");
        gli_delete_window(w, NULL);
    }

    /* ---- Stream destruction with NULL callbacks succeeds ---- */

    {
        stream_t *s = gli_new_stream(strtype_Memory, 1, 1, 0);
        TEST_ASSERT(s != NULL, "created stream for NULL-callback delete test");
        /* Should not crash */
        gli_delete_stream(s, NULL);
        TEST_ASSERT(1, "gli_delete_stream() with NULL callbacks does not crash");
    }

    /* ---- Window destruction with NULL callbacks succeeds ---- */

    {
        window_t *w = gli_new_window(0);
        TEST_ASSERT(w != NULL, "created window for NULL-callback delete test");
        /* Should not crash */
        gli_delete_window(w, NULL);
        TEST_ASSERT(1, "gli_delete_window() with NULL callbacks does not crash");
    }

    /* ---- gidispatch_set_object_registry stores callbacks ---- */

    mock_reset();
    gidispatch_set_object_registry(mock_register_obj, mock_unregister_obj);
    TEST_ASSERT(gli_register_obj == mock_register_obj,
        "gli_register_obj set to mock_register_obj");
    TEST_ASSERT(gli_unregister_obj == mock_unregister_obj,
        "gli_unregister_obj set to mock_unregister_obj");

    /* ---- Object registration callback invoked on stream creation ---- */

    mock_reset();
    mock_regi_result.num = 42;

    {
        stream_t *s = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(s != NULL, "created stream for registration test");
        TEST_ASSERT(mock_regi_called == 1,
            "mock_register_obj called once on stream creation");
        TEST_ASSERT(mock_regi_obj == s,
            "mock_register_obj received correct stream pointer");
        TEST_ASSERT(mock_regi_class == gidisp_Class_Stream,
            "mock_register_obj received gidisp_Class_Stream");
        TEST_ASSERT(s->disprock.num == 42,
            "stream disprock set to value returned by mock_register_obj");

        /* Clean up */
        gli_delete_stream(s, NULL);
    }

    /* ---- Object unregistration callback invoked on stream destruction ---- */

    mock_reset();
    mock_regi_result.num = 99;

    {
        stream_t *s = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(s != NULL, "created stream for unregistration test");
        TEST_ASSERT(mock_regi_called == 1,
            "mock_register_obj called on stream creation");

        /* Reset mock counters to track unregister separately */
        mock_regi_called = 0;
        mock_regi_obj = NULL;
        mock_regi_class = 0xFFFFFFFF;

        gli_delete_stream(s, NULL);

        TEST_ASSERT(mock_unregi_called == 1,
            "mock_unregister_obj called once on stream destruction");
        TEST_ASSERT(mock_unregi_obj == s,
            "mock_unregister_obj received correct stream pointer");
        TEST_ASSERT(mock_unregi_class == gidisp_Class_Stream,
            "mock_unregister_obj received gidisp_Class_Stream");
        TEST_ASSERT(mock_unregi_rock.num == 99,
            "mock_unregister_obj received correct disprock");
    }

    /* ---- Object registration callback invoked on window creation
     *
     * NOTE: gli_new_window() internally calls gli_new_stream() to create
     * the window's owned stream. This means mock_register_obj is called
     * TWICE: first for the stream (gidisp_Class_Stream), then for the
     * window (gidisp_Class_Window). The last call should be for the window.
     * ---- */

    mock_reset();
    mock_regi_result.num = 77;

    {
        window_t *w = gli_new_window(0);
        TEST_ASSERT(w != NULL, "created window for registration test");
        TEST_ASSERT(mock_regi_called == 2,
            "mock_register_obj called twice (stream + window) on window creation");
        /* The last registration call should be for the window */
        TEST_ASSERT(mock_regi_obj == w,
            "mock_register_obj last call received correct window pointer");
        TEST_ASSERT(mock_regi_class == gidisp_Class_Window,
            "mock_register_obj last call received gidisp_Class_Window");
        TEST_ASSERT(w->disprock.num == 77,
            "window disprock set to value returned by mock_register_obj");

        /* Clean up */
        gli_delete_window(w, NULL);
    }

    /* ---- Object unregistration callback invoked on window destruction
     *
     * NOTE: gli_delete_window() internally calls gli_delete_stream()
     * to destroy the owned stream. This means mock_unregister_obj is called
     * TWICE: first for the window (gidisp_Class_Window), then for the
     * owned stream (gidisp_Class_Stream). The mock stores only the last
     * call's arguments, so we verify the count and the rock value (which
     * is the same for both window and stream in this test).
     * ---- */

    mock_reset();
    mock_regi_result.num = 55;

    {
        window_t *w = gli_new_window(0);
        TEST_ASSERT(w != NULL, "created window for unregistration test");
        /* mock_register was called for both stream and window */
        TEST_ASSERT(mock_regi_called == 2,
            "mock_register_obj called twice on window creation");

        /* Reset mock counters to track unregister separately */
        mock_regi_called = 0;
        mock_regi_obj = NULL;
        mock_regi_class = 0xFFFFFFFF;

        gli_delete_window(w, NULL);

        /* Two unregisters: first for window, then for the owned stream */
        TEST_ASSERT(mock_unregi_called == 2,
            "mock_unregister_obj called twice (window + stream) on window destruction");
        /* The last unregister call is for the stream, so mock_unregi_class
         * will be gidisp_Class_Stream. Both calls pass rock.num == 55. */
        TEST_ASSERT(mock_unregi_rock.num == 55,
            "mock_unregister_obj received correct disprock (55)");
    }

    /* ---- gidispatch_get_objrock returns correct disprock for streams ---- */

    mock_reset();
    mock_regi_result.num = 123;

    {
        stream_t *s = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(s != NULL, "created stream for get_objrock test");

        gidispatch_rock_t rock = gidispatch_get_objrock(s, gidisp_Class_Stream);
        TEST_ASSERT(rock.num == 123,
            "gidispatch_get_objrock returns correct stream disprock");

        /* Unknown stream pointer returns zeroed rock */
        stream_t fake;
        fake.disprock.num = 0;
        rock = gidispatch_get_objrock(&fake, gidisp_Class_Stream);
        TEST_ASSERT(rock.num == 0,
            "gidispatch_get_objrock returns 0 for unknown stream");

        gli_delete_stream(s, NULL);
    }

    /* ---- gidispatch_get_objrock returns correct disprock for windows ---- */

    mock_reset();
    mock_regi_result.num = 456;

    {
        window_t *w = gli_new_window(0);
        TEST_ASSERT(w != NULL, "created window for get_objrock test");

        gidispatch_rock_t rock = gidispatch_get_objrock(w, gidisp_Class_Window);
        TEST_ASSERT(rock.num == 456,
            "gidispatch_get_objrock returns correct window disprock");

        /* Unknown window pointer returns zeroed rock */
        window_t fake;
        fake.disprock.num = 0;
        rock = gidispatch_get_objrock(&fake, gidisp_Class_Window);
        TEST_ASSERT(rock.num == 0,
            "gidispatch_get_objrock returns 0 for unknown window");

        gli_delete_window(w, NULL);
    }

    /* ---- gidispatch_get_objrock returns 0 for unknown classes ---- */

    {
        gidispatch_rock_t rock = gidispatch_get_objrock(NULL, gidisp_Class_Fileref);
        TEST_ASSERT(rock.num == 0,
            "gidispatch_get_objrock returns 0 for fileref class (not implemented)");

        rock = gidispatch_get_objrock(NULL, gidisp_Class_Schannel);
        TEST_ASSERT(rock.num == 0,
            "gidispatch_get_objrock returns 0 for schannel class (not implemented)");

        rock = gidispatch_get_objrock(NULL, 999);
        TEST_ASSERT(rock.num == 0,
            "gidispatch_get_objrock returns 0 for unknown class");
    }

    /* ---- Restore NULL callbacks for subsequent tests ---- */

    gidispatch_set_object_registry(NULL, NULL);
    TEST_ASSERT(gli_register_obj == NULL,
        "gli_register_obj restored to NULL");
    TEST_ASSERT(gli_unregister_obj == NULL,
        "gli_unregister_obj restored to NULL");

    return 0;
}