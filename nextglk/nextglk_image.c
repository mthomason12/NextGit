/*
 * nextglk_image.c — Image subsystem stubs for NextGit.
 *
 * This file implements the Glk image API as a compileable stub.
 * All draw functions return FALSE; glk_image_get_info zeros
 * width/height and returns FALSE.
 *
 * TODO — Spectrum Next implementation plan:
 *   - Blorb resource lookup: parse Blorb chunks to locate image
 *     data by resource number (loose/ChunkID "Rect" / "PNG ").
 *   - SD card loading: read image data from SD card filesystem
 *     via ESXDOS or NextZXOS API.
 *   - Layer 2 display: convert decoded image data to Layer 2
 *     framebuffer format (256×192, 8-bit indexed or true colour)
 *     at the requested window coordinates.
 *   - Optional Raspberry Pi acceleration: offload blitting and
 *     scaling to the Pi co-processor over SPI for higher
 *     framebuffer throughput.
 */

#include "glk.h"

#ifdef GLK_MODULE_IMAGE

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
    (void)win;
    (void)image;
    (void)val1;
    (void)val2;

    return 0;
}

glui32 glk_image_draw_scaled(winid_t win, glui32 image,
    glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
    (void)win;
    (void)image;
    (void)val1;
    (void)val2;
    (void)width;
    (void)height;

    return 0;
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
    (void)image;

    if (width)
        *width = 0;
    if (height)
        *height = 0;

    return 0;
}

#ifdef GLK_MODULE_IMAGE2
glui32 glk_image_draw_scaled_ext(winid_t win, glui32 image,
    glsi32 val1, glsi32 val2, glui32 width, glui32 height,
    glui32 imagerule, glui32 maxwidth)
{
    (void)win;
    (void)image;
    (void)val1;
    (void)val2;
    (void)width;
    (void)height;
    (void)imagerule;
    (void)maxwidth;

    return 0;
}
#endif /* GLK_MODULE_IMAGE2 */

#endif /* GLK_MODULE_IMAGE */