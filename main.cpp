#include "nbrenderer.h"
#include "camera.h"
#include "hittable.h"
#include "hittable_list.h"
#include "sphere.h"

int main() {

    std::freopen("ImageOutputFiles/image.ppm", "w", stdout);

    // WORLD
    hittable_list world;
    auto material_ground = make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_left   = make_shared<metal>(color(0.8, 0.8, 0.8));
    auto material_right  = make_shared<metal>(color(0.8, 0.6, 0.2));

    world.add(make_shared<sphere>(point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(make_shared<sphere>(point3( 0.0,    0.0, -1.2),   0.5, material_center));
    world.add(make_shared<sphere>(point3(-1.5,    0.0, -1.5),   0.5, material_left));
    world.add(make_shared<sphere>(point3( 1.1,    0.0, -1.25),   0.5, material_right));

    // CAMERA
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width  = 640;
    cam.samples_per_pixel = 20;
    cam.max_depth = 8;

    cam.render(world);
}
