//
// Created by Nutbotty on 7/15/2026.
//

#ifndef NB_RENDERER_COLOR_H
#define NB_RENDERER_COLOR_H

#include "vec3.h"

#include <iostream>

using color = vec3;

void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    //translate to byte range
    int rbyte = int(255.999 * r);
    int gbyte = int(255.999 * g);
    int bbyte = int(255.999 * b);

    // write color components to out
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

#endif //NB_RENDERER_COLOR_H
