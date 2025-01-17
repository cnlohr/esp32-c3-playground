//==============================================================================
// Includes
//==============================================================================

#include "display.h"
#include "bresenham.h"
#include "aabb_utils.h"

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 * @param disp
 * @param box
 * @param color
 * @param scalingFactor
 */
void drawBox(display_t* disp, box_t box, paletteColor_t color, bool isFilled, int32_t scalingFactor)
{
    if(isFilled)
    {
        fillDisplayArea(disp,
                        box.x0 / scalingFactor,
                        box.y0 / scalingFactor,
                        box.x1 / scalingFactor,
                        box.y1 / scalingFactor,
                        color);
    }
    else
    {
        plotRect(disp,
                 box.x0 / scalingFactor,
                 box.y0 / scalingFactor,
                 box.x1 / scalingFactor,
                 box.y1 / scalingFactor,
                 color);
    }
}

/**
 * @brief
 *
 * @param box0 A box to check for collision
 * @param box1 The other box to check for collision
 * @param scalingFactor The factor to scale the boxes by before checking for collision
 * @return true if the boxes collide, false if they do not
 */
bool boxesCollide(box_t box0, box_t box1, int32_t scalingFactor)
{
    return (box0.x0 / scalingFactor) < (box1.x1 / scalingFactor) &&
           (box0.x1 / scalingFactor) > (box1.x0 / scalingFactor) &&
           (box0.y0 / scalingFactor) < (box1.y1 / scalingFactor) &&
           (box0.y1 / scalingFactor) > (box1.y0 / scalingFactor);
}