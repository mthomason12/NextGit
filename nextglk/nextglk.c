/* nextglk.c — NextGlk lifecycle and dispatch-layer bridge
 *
 * Implements the Glk library functions that are part of the dispatch ABI
 * but not part of the window/stream/input subsystems:
 *
 *   gidispatch_set_object_registry()   — store VM-supplied registration callbacks
 *   gidispatch_get_objrock()           — retrieve a gidispatch_rock_t for an object
 *
 * Defines the dispatch callback function pointers used by next_stream.c and
 * next_window.c to register/unregister objects with the Git VM's dispatch layer.
 */

#include <stdbool.h>
#include "nextglk_internal.h"

/* -------------------------------------------------------------------------
 * Dispatch-layer registration callbacks
 *
 * These function pointers are set by the Git VM via
 * gidispatch_set_object_registry() during startup.
 * They may be NULL before the VM initialises the dispatch layer.
 *
 * Defined here, declared extern in nextglk_internal.h.
 * ------------------------------------------------------------------------- */

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock) = NULL;

/* -------------------------------------------------------------------------
 * gidispatch_set_object_registry — Store VM dispatch callbacks
 *
 * Called by the Git VM during startup to register its object-registration
 * and unregistration callbacks. These callbacks are stored in the global
 * function pointers gli_register_obj and gli_unregister_obj, which are
 * called by gli_new_stream(), gli_delete_stream(), gli_new_window(),
 * and gli_delete_window() when objects are created or destroyed.
 *
 * Parameters:
 *   regi   — callback to invoke when an opaque object is created
 *   unregi — callback to invoke when an opaque object is destroyed
 * ------------------------------------------------------------------------- */

void gidispatch_set_object_registry(
    gidispatch_rock_t (*regi)(void *obj, glui32 objclass),
    void (*unregi)(void *obj, glui32 objclass, gidispatch_rock_t objrock))
{
    gli_register_obj = regi;
    gli_unregister_obj = unregi;
}

/* -------------------------------------------------------------------------
 * gidispatch_get_objrock — Return the disprock for an opaque object
 *
 * Searches the known object lists (streams, windows) for the given pointer
 * and returns its gidispatch_rock_t. This function is called by the Git VM
 * during dispatch-layer marshalling to locate the rock value associated with
 * an opaque object reference.
 *
 * If the object is not found (or the class is unknown), returns a zeroed
 * gidispatch_rock_t.
 *
 * Parameters:
 *   obj      — pointer to the opaque object (stream_t, window_t, etc.)
 *   objclass — class ID constant (gidisp_Class_Stream, gidisp_Class_Window, etc.)
 *
 * Returns:
 *   The object's disprock, or zeroed if not found.
 * ------------------------------------------------------------------------- */

gidispatch_rock_t gidispatch_get_objrock(void *obj, glui32 objclass)
{
    gidispatch_rock_t rock;
    rock.num = 0;

    switch (objclass) {
        case gidisp_Class_Stream: {
            stream_t *str = gli_streamlist;
            while (str) {
                if (str == obj)
                    return str->disprock;
                str = str->next;
            }
            break;
        }
        case gidisp_Class_Window: {
            if (gli_mainwin && gli_mainwin == obj)
                return gli_mainwin->disprock;
            break;
        }
        case gidisp_Class_Fileref:
        case gidisp_Class_Schannel:
        default:
            /* Not yet implemented — return zeroed rock */
            break;
    }

    return rock;
}

/* -------------------------------------------------------------------------
 * nextglk_init — Initialise the NextGlk library
 *
 * Called by the platform layer during startup, before the Git VM begins.
 * Currently returns true with no additional setup required.
 *
 * Returns:
 *   true on success, false on failure
 * ------------------------------------------------------------------------- */

bool nextglk_init(void)
{
    return true;
}

/* -------------------------------------------------------------------------
 * nextglk_shutdown — Shut down the NextGlk library
 *
 * Called by the platform layer during shutdown, after the Git VM exits.
 * Currently a no-op.
 * ------------------------------------------------------------------------- */

void nextglk_shutdown(void)
{
}