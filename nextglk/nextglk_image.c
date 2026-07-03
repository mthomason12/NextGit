#include "nextglk.h"

bool nextglk_image_init(void)
{
    return true;
}

bool nextglk_image_show(uint32_t image_id)
{
    (void)image_id;

    return false;
}

bool nextglk_image_get_info(
    uint32_t image_id,
    uint16_t *width,
    uint16_t *height)
{
    (void)image_id;

    if (width)
    {
        *width = 0;
    }

    if (height)
    {
        *height = 0;
    }

    return false;
}

void nextglk_image_clear(void)
{
}