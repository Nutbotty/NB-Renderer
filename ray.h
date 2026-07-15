//
// Created by Nutbotty on 7/15/2026.
//

#ifndef NB_RENDERER_RAY_H
#define NB_RENDERER_RAY_H

#include "vec3.h"

class ray {
    public:
        ray() {}

        ray(const point3& origin, const vec3& direction) : orig(origin), dir(direction) {}

        const point3& origin() const { return orig; }
        const vec3& direction() const { return dir; }

        point3 at(double t) const {
            return orig + t*dir;
        }

    private:
        point3 orig;
        vec3 dir;
};

#endif //NB_RENDERER_RAY_H
