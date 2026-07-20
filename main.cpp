#include "CPURenderer/nbrenderer.h"

#include "CPURenderer/bvh.h"
#include "CPURenderer/hittable.h"
#include "CPURenderer/hittable_list.h"
#include "CPURenderer/sphere.h"
#include "CPURenderer/material.h"
#include "CPURenderer/texture.h"
#include "OpenGLRenderer/openglviewport.h"
#include <chrono>
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#include "OpenGLRenderer/editorcamera.h"
#include "Scene/scene.h"
#include "CPURenderer/CPURenderer.h"



void bouncing_spheres(hittable_list& world, Scene& scene) {


    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));
    scene.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(checker)));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    scene.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    scene.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    scene.add(make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    scene.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    scene.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    scene.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));


    for (const auto& object : scene.GetObjects())
    {
        world.add(object);
    }
    world = hittable_list(make_shared<bvh_node>(world));
    // world = scene.GetObjects();


    // CAMERA
    EditorCamera editorCamera = viewport(scene);
    CPURenderer my_cpu_renderer;
    camera cam = my_cpu_renderer.cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width  = 640;
    cam.samples_per_pixel = 50;
    cam.max_depth = 8;

    cam.vfov     = 20;
    float focus = editorCamera.FocusDistance;

    glm::vec3 target =
        editorCamera.Position +
        editorCamera.Front * focus;


    cam.lookfrom = point3(
        editorCamera.Position.x,
        editorCamera.Position.y,
        editorCamera.Position.z
    );


    cam.lookat = point3(
        target.x,
        target.y,
        target.z
    );

    cam.vup = vec3(
        editorCamera.Up.x,
        editorCamera.Up.y,
        editorCamera.Up.z
    );

    // cam.defocus_angle = editorCamera.DefocusAngle;
    // cam.focus_dist = editorCamera.FocusDistance;
    cam.defocus_angle = 4.0;
    cam.focus_dist    = 10.0;

    auto start = std::chrono::steady_clock::now();
    cam.render(world);
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cerr << "\nRender time: "
          << elapsed.count()
          << " ms ("
          << elapsed.count() / 1000.0
          << " s)\n";
}

void checkered_spheres(hittable_list& world, Scene& scene) {
    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));
    scene.add(make_shared<sphere>(point3(0,-10, 0), 10, make_shared<lambertian>(checker)));

    scene.add(make_shared<sphere>(point3(0, 10, 0), 10, make_shared<lambertian>(checker)));

    for (const auto& object : scene.GetObjects())
    {
        world.add(object);
    }
    world = hittable_list(make_shared<bvh_node>(world));

    EditorCamera editorCamera = viewport(scene);
    CPURenderer my_cpu_renderer;
    camera cam = my_cpu_renderer.cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;

    cam.vfov     = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void earth(hittable_list& world, Scene& scene) {
    auto earth_texture = make_shared<image_texture>("external/earthmap.jpg");
    auto earth_surface = make_shared<lambertian>(earth_texture);
    scene.add(make_shared<sphere>(point3(0,0,0), 2, earth_surface));

    for (const auto& object : scene.GetObjects())
    {
        world.add(object);
    }
    world = hittable_list(make_shared<bvh_node>(world));
    EditorCamera editorCamera = viewport(scene);
    CPURenderer my_cpu_renderer;
    camera cam = my_cpu_renderer.cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;

    cam.vfov     = 20;
    float focus = editorCamera.FocusDistance;
    glm::vec3 target =
        editorCamera.Position +
        editorCamera.Front * focus;


    cam.lookfrom = point3(
        editorCamera.Position.x,
        editorCamera.Position.y,
        editorCamera.Position.z
    );


    cam.lookat = point3(
        target.x,
        target.y,
        target.z
    );

    cam.vup = vec3(
        editorCamera.Up.x,
        editorCamera.Up.y,
        editorCamera.Up.z
    );

    cam.defocus_angle = 0;

    cam.render(world);
}

void perlin_spheres(hittable_list& world, Scene& scene) {

    auto pertext = make_shared<noise_texture>(4);
    scene.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(pertext)));
    scene.add(make_shared<sphere>(point3(0,2,0), 2, make_shared<lambertian>(pertext)));

    for (const auto& object : scene.GetObjects())
    {
        world.add(object);
    }
    world = hittable_list(make_shared<bvh_node>(world));
    EditorCamera editorCamera = viewport(scene);
    CPURenderer my_cpu_renderer;
    camera cam = my_cpu_renderer.cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;

    cam.vfov     = 20;
    float focus = editorCamera.FocusDistance;
    glm::vec3 target =
        editorCamera.Position +
        editorCamera.Front * focus;


    cam.lookfrom = point3(
        editorCamera.Position.x,
        editorCamera.Position.y,
        editorCamera.Position.z
    );


    cam.lookat = point3(
        target.x,
        target.y,
        target.z
    );

    cam.vup = vec3(
        editorCamera.Up.x,
        editorCamera.Up.y,
        editorCamera.Up.z
    );

    cam.defocus_angle = 0;

    cam.render(world);
}


int main() {
    std::freopen("ImageOutputFiles/image.ppm", "w", stdout);

    // WORLD
    hittable_list world;
    Scene scene;
    switch (4) {
        case 1: bouncing_spheres(world, scene); break;
        case 2: checkered_spheres(world, scene); break;
        case 3: earth(world, scene); break;
        case 4: perlin_spheres(world, scene); break;
    }
}
