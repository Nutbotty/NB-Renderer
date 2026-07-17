#include "nbrenderer.h"
#include "camera.h"
#include "hittable.h"
#include "hittable_list.h"
#include "sphere.h"

int main() {

    std::freopen("ImageOutputFiles/image.ppm", "w", stdout);

    // WORLD
    hittable_list world;
    auto material_ground = make_shared<lambertian>(color(0.16, 0.16, 0.16));
    auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_left   = make_shared<dielectric>(1.5);
    auto material_bubble_outside = make_shared<dielectric>(1.50);
    auto material_bubble_inside = make_shared<dielectric>(1.00 / 1.50);
    auto material_right  = make_shared<metal>(color(0.8, 0.6, 0.2), 0.3);

    world.add(make_shared<sphere>(point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(make_shared<sphere>(point3( 0.0,    0.0, -1.2),   0.5, material_center));
    world.add(make_shared<sphere>(point3( -1.0,    1.0, -1.4),   0.5, material_bubble_outside));
    world.add(make_shared<sphere>(point3( -1.0,    1.0, -1.4),   0.4, material_bubble_inside));
    world.add(make_shared<sphere>(point3(-1.5,    0.0, -1.8),   0.5, material_left));
    world.add(make_shared<sphere>(point3( 1.2,    0.0, -1.25),   0.5, material_right));

    // CAMERA
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width  = 640;
    cam.samples_per_pixel = 20;
    cam.max_depth = 8;

    cam.vfov = 37.85; // can convert to focal length (lens size)
    cam.lookfrom = point3(-1,1,1);
    cam.lookat   = point3(0,0,-1);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 8.0;
    cam.focus_dist    = 2.575;

    cam.render(world);
}
