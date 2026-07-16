#include "nbrenderer.h"
#include "camera.h"
#include "hittable.h"
#include "hittable_list.h"
#include "sphere.h"

int main() {

    std::freopen("ImageOutputFiles/image.ppm", "w", stdout);

    // WORLD
    hittable_list world;
    world.add(make_shared<sphere>(point3(0, 0, -1), 0.5));
    world.add(make_shared<sphere>(point3(0, -100.5, -1), 100));

    // CAMERA
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width  = 32;

    cam.render(world);
}
