//
// Created by Nutbotty on 7/15/2026.
//

#ifndef NB_RENDERER_NBRENDERER_H
#define NB_RENDERER_NBRENDERER_H

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>


// C++ Std Usings

using std::make_shared;
using std::shared_ptr;

// Constants

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

// Common Headers

#include "color.h"
#include "ray.h"
#include "vec3.h"

#endif //NB_RENDERER_NBRENDERER_H
