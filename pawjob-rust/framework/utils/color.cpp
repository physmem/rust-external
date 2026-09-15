#include "color.hpp"
#include <cmath>

Color Hsv::to_color()
{
    float r, g, b;

    int i = static_cast<int>(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
    default: r = 0, g = 0, b = 0; break;
    }

    return Color(
        static_cast<std::uint8_t>(r * 255),
        static_cast<std::uint8_t>(g * 255),
        static_cast<std::uint8_t>(b * 255)
    );
}

Hsv Color::to_hsv() const
{
    Hsv out;
    float r = this->r / 255.0f;
    float g = this->g / 255.0f;
    float b = this->b / 255.0f;

    float max = std::max({ r, g, b });
    float min = std::min({ r, g, b });
    float diff = max - min;

    out.v = max;

    if (max > 0.0f) {
        out.s = diff / max;
    } else {
        out.s = 0.0f;
        out.h = 0.0f;
        return out;
    }

    if (max == min) {
        out.h = 0.0f;
    } else {
        if (max == r) {
            out.h = (g - b) / diff + (g < b ? 6 : 0);
        } else if (max == g) {
            out.h = (b - r) / diff + 2;
        } else if (max == b) {
            out.h = (r - g) / diff + 4;
        }
        out.h /= 6.0f;
    }

    return out;
}
