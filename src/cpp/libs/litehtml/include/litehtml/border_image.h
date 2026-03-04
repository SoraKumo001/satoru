#ifndef LH_BORDER_IMAGE_H
#define LH_BORDER_IMAGE_H

#include "css_length.h"
#include "types.h"
#include "web_color.h"
#include "gradient.h"

namespace litehtml
{
    enum border_image_repeat
    {
        border_image_repeat_stretch,
        border_image_repeat_repeat,
        border_image_repeat_round,
        border_image_repeat_space
    };

    struct border_image
    {
        image source;
        string baseurl;
        css_length slice[4];
        bool slice_fill;
        css_length width[4];
        css_length outset[4];
        border_image_repeat repeat_h;
        border_image_repeat repeat_v;

        border_image()
        {
            slice_fill = false;
            repeat_h = border_image_repeat_stretch;
            repeat_v = border_image_repeat_stretch;
            for (int i = 0; i < 4; i++)
            {
                slice[i] = css_length(100, css_units_percentage);
                width[i] = css_length(1, css_units_none);
                outset[i] = css_length(0);
            }
        }

        bool is_valid() const
        {
            return !source.is_empty();
        }
    };

    const string border_image_repeat_strings = "stretch;repeat;round;space";
}

#endif // LH_BORDER_IMAGE_H
