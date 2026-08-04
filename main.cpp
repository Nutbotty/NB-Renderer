#include "CPURenderer/nbrenderer.h"

#include "CPURenderer/bvh.h"
#include "CPURenderer/hittable.h"
#include "CPURenderer/hittable_list.h"
#include "CPURenderer/sphere.h"
#include "CPURenderer/Quad.h"
#include "CPURenderer/Tri.h"
#include "CPURenderer/material.h"
#include "CPURenderer/texture.h"
#include "OpenGLRenderer/EditorOpenGL.h"
#include <chrono>
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#include "OpenGLRenderer/editorcamera.h"
#include "Scene/scene.h"
#include "CPURenderer/CPURenderer.h"
#include "OpenGLRenderer/OpenGLRenderer.h"

std::vector<std::shared_ptr<material>>

BuildCpuMaterials(const Scene& scene)
{
    std::vector<std::shared_ptr<material>> cpuMaterials;

    cpuMaterials.reserve(
        scene.GetMaterials().size()
    );

    for (const SceneMaterial& source : scene.GetMaterials())
    {
        switch (source.type)
        {
            case MaterialType::Lambertian:
            {
                cpuMaterials.push_back(
                    std::make_shared<lambertian>(
                        color(
                            source.albedo.r,
                            source.albedo.g,
                            source.albedo.b
                        )
                    )
                );

                break;
            }

            case MaterialType::Metal:
            {
                cpuMaterials.push_back(
                    std::make_shared<metal>(
                        color(
                            source.albedo.r,
                            source.albedo.g,
                            source.albedo.b
                        ),
                        source.fuzz
                    )
                );

                break;
            }

            case MaterialType::Dielectric:
            {
                cpuMaterials.push_back(
                    std::make_shared<dielectric>(
                        source.indexOfRefraction
                    )
                );

                break;
            }
        }
    }

    return cpuMaterials;
}
hittable_list BuildCpuWorld(const Scene& scene)
{
    hittable_list world;

    const auto cpuMaterials =
        BuildCpuMaterials(scene);

    for (const SceneSphere& source : scene.GetSpheres())
    {
        world.add(
            std::make_shared<sphere>(
                point3(source.center.x, source.center.y, source.center.z),
                source.radius,
                cpuMaterials[source.material]
            )
        );
    }

    for (const SceneQuad& source : scene.GetQuads())
    {
        world.add(
            std::make_shared<quad>(
                point3(source.Q.x,source.Q.y,source.Q.z),
                vec3(source.u.x,source.u.y,source.u.z),
                vec3(source.v.x,source.v.y,source.v.z),
                cpuMaterials[source.material]
            )
        );
    }

    for (const SceneTri& source : scene.GetTris())
    {
        world.add(
            std::make_shared<tri>(
                point3(source.Q.x,source.Q.y,source.Q.z),
                vec3(source.U.x,source.U.y,source.U.z),
                vec3(source.V.x,source.V.y,source.V.z),
                cpuMaterials[source.material]
            )
        );
    }

    return world;
}

void quads(hittable_list& world, Scene& scene) {
    scene.Clear();
    const MaterialId groundMaterial = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.5f, 0.5f, 0.5f),0, 1.0, glm::vec3(1.0f, 0.8f, 0.6f), 2.0f);
    const MaterialId mat1 = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.2f, 1.0f, 0.2f));
    const MaterialId mat2 = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.2f, 0.2f, 1.0f));
    const MaterialId mat3 = scene.addMaterial(MaterialType::Lambertian,glm::vec3(1.0f, 0.5f, 0.0f));
    const MaterialId mat4 = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.2f, 0.8f, 0.8f));
    scene.addQuad(glm::vec3(-3, -2, 5), glm::vec3(0, 0, -4), glm::vec3(0, 4, 0), groundMaterial);
    scene.addTri(glm::vec3(-2, -2, 0), glm::vec3(4, 0, 0), glm::vec3(0, 4, 0), mat1);
    scene.addTri(glm::vec3(2, 2, 0), glm::vec3(-4, 0, 0), glm::vec3(0, -4, 0), mat2);
    scene.addQuad(glm::vec3(3, 3, 1), glm::vec3(0, 0, 4), glm::vec3(0, 4, 0), mat2);
    scene.addQuad(glm::vec3(-2, 3, 1), glm::vec3(4, 0, 0), glm::vec3(0, 0, 4), mat3);
    scene.addQuad(glm::vec3(-2, -3, 5), glm::vec3(4, 0, 0), glm::vec3(0, 0, -4), mat4);
    world = BuildCpuWorld(scene);
    auto bvhRoot = std::make_shared<bvh_node>(world);
    world = hittable_list(bvhRoot);

    SceneCamera sceneCamera;
    sceneCamera.lookFrom = glm::vec3(0, 0, 9);
    sceneCamera.lookAt = glm::vec3(0, 0, 0);
    sceneCamera.up = glm::vec3(0, 1, 0);
    sceneCamera.verticalFovDegrees = 80.0f;
    sceneCamera.defocusAngleDegrees = 0.0f;
    sceneCamera.focusDistance = 10.0f;
    scene.SetCamera(sceneCamera);
    CPURenderer cpuRenderer;
    camera cam = cpuRenderer.cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 640;
    cam.samples_per_pixel = 50;
    cam.max_depth = 8;

    const SceneCamera& cameraData = scene.GetCamera();

    cam.vfov = cameraData.verticalFovDegrees;
    cam.lookfrom = point3(cameraData.lookFrom.x,cameraData.lookFrom.y,cameraData.lookFrom.z);
    cam.lookat = point3(cameraData.lookAt.x,cameraData.lookAt.y,cameraData.lookAt.z);
    cam.vup = vec3(cameraData.up.x,cameraData.up.y,cameraData.up.z);
    cam.defocus_angle = cameraData.defocusAngleDegrees;
    cam.focus_dist = cameraData.focusDistance;
    viewport(scene);
    // cam.render(world);
}

void cornell(hittable_list& world, Scene& scene)  {
    scene.Clear();
    const MaterialId light = scene.addMaterial(MaterialType::Lambertian,glm::vec3(1.0f, 1.0f, 1.0f),0, 1.0, glm::vec3(1.0f, 1.0f, 1.0f), 60.0f);
    const MaterialId green = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.12f, 0.45f, 0.15f),0, 1.0, glm::vec3(0.12f, 0.45f, 0.15f), 0.0f);
    const MaterialId red = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.65f, 0.05f, 0.05f),0, 1.0, glm::vec3(0.65f, 0.05f, 0.05f), 0.0f);
    const MaterialId white = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.73f, 0.73f, 0.73f),0, 1.0, glm::vec3(0.73f, 0.73f, 0.73f), 0.0f);
    scene.addQuad(glm::vec3(555, 0, 0), glm::vec3(0, 555, 0), glm::vec3(0, 0, 555), green);
    QuadId myQuad = scene.addQuad(glm::vec3(0, 0, 0), glm::vec3(0, 555, 0), glm::vec3(0, 0, 555), red);
    scene.addQuad(glm::vec3(343, 554, 332), glm::vec3(-130, 0, 0), glm::vec3(0, 0, -105), light);
    scene.addQuad(glm::vec3(0, 0, 0), glm::vec3(555, 0, 0), glm::vec3(0, 0, 555), white);
    scene.addQuad(glm::vec3(555, 555, 555), glm::vec3(-555, 0, 0), glm::vec3(0, 0, -555), white);
    scene.addQuad(glm::vec3(0, 0, 555), glm::vec3(555, 0, 0), glm::vec3(0, 555, 0), white);

    scene.addQuad(glm::vec3(295,0,65),  glm::vec3(0,165,0), glm::vec3(0,0,165), white);
    scene.addQuad(glm::vec3(130,0,230), glm::vec3(0,165,0), glm::vec3(0,0,-165), white);
    scene.addQuad(glm::vec3(130,165,230), glm::vec3(165,0,0), glm::vec3(0,0,-165), white);
    scene.addQuad(glm::vec3(130,0,65), glm::vec3(165,0,0), glm::vec3(0,0,165), white);
    scene.addQuad(glm::vec3(130,0,230), glm::vec3(165,0,0), glm::vec3(0,165,0), white);
    scene.addQuad(glm::vec3(295,0,65), glm::vec3(-165,0,0), glm::vec3(0,165,0), white);
    scene.addQuad(glm::vec3(430,0,295), glm::vec3(0,330,0), glm::vec3(0,0,165), white);
    scene.addQuad(glm::vec3(265,0,460), glm::vec3(0,330,0), glm::vec3(0,0,-165), white);
    scene.addQuad(glm::vec3(265,330,460), glm::vec3(165,0,0), glm::vec3(0,0,-165), white);
    scene.addQuad(glm::vec3(265,0,295), glm::vec3(165,0,0), glm::vec3(0,0,165), white);
    scene.addQuad(glm::vec3(265,0,460), glm::vec3(165,0,0), glm::vec3(0,330,0), white);
    scene.addQuad(glm::vec3(430,0,295), glm::vec3(-165,0,0), glm::vec3(0,330,0), white);
    glm::mat4 transform{1.0f};

    transform = glm::translate(transform,glm::vec3(5.0f, 0.0f, 0.0f));
    transform = glm::rotate(transform,glm::radians(30.0f),glm::vec3(0.0f, 1.0f, 0.0f));
    scene.addInstance(ScenePrimitiveRecord{ScenePrimitiveType::Quad,myQuad},transform);
    glm::mat4 testTransform{1.0f};

    testTransform = glm::translate(
        testTransform,
        glm::vec3(-150.0f, 100.0f, 0.0f)
    );

    testTransform = glm::rotate(
        testTransform,
        glm::radians(35.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    scene.addInstance(
        {
            ScenePrimitiveType::Quad,
            myQuad
        },
        testTransform
    );



    SceneCamera sceneCamera;
    sceneCamera.lookFrom = glm::vec3(278, 278, -800);
    sceneCamera.lookAt = glm::vec3(278, 278, 0);
    sceneCamera.up = glm::vec3(0, 1, 0);
    sceneCamera.verticalFovDegrees = 40.0f;
    sceneCamera.defocusAngleDegrees = 0.0f;
    sceneCamera.focusDistance = 10.0f;
    scene.SetCamera(sceneCamera);
    viewport(scene);
    // Renderer(scene);
}

void bouncing_spheres(hittable_list& world, Scene& scene) {
    scene.Clear();
    const MaterialId groundMaterial = scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.5f, 0.5f, 0.5f));
    scene.addSphere(glm::vec3(0.0f, -1000.0f, 0.0f),1000.0f,groundMaterial);

    for (int a = -11; a < 11; ++a)
    {
        for (int b = -11; b < 11; ++b)
        {
            const double chooseMaterial =
                random_double();

            const glm::vec3 center(
                static_cast<float>(a)
                    + 0.9f
                    * static_cast<float>(random_double()),
                0.2f,
                static_cast<float>(b)
                    + 0.9f
                    * static_cast<float>(random_double())
            );

            const float distanceFromLargeSphere =
                glm::length(
                    center
                    - glm::vec3(4.0f, 0.2f, 0.0f)
                );

            if (distanceFromLargeSphere <= 0.9f)
            {
                continue;
            }

            MaterialId sphereMaterial;

            if (chooseMaterial < 0.8)
            {
                /*
                 * Diffuse:
                 *
                 * RTIOW uses:
                 * color::random() * color::random()
                 */
                const glm::vec3 randomColorA(
                    static_cast<float>(random_double()),
                    static_cast<float>(random_double()),
                    static_cast<float>(random_double())
                );

                const glm::vec3 randomColorB(
                    static_cast<float>(random_double()),
                    static_cast<float>(random_double()),
                    static_cast<float>(random_double())
                );

                const glm::vec3 albedo =
                    randomColorA * randomColorB;

                sphereMaterial =
                    scene.addMaterial(
                        MaterialType::Lambertian,
                        albedo
                    );
            }
            else if (chooseMaterial < 0.95)
            {
                /*
                 * Metal.
                 */
                const glm::vec3 albedo(
                    static_cast<float>(
                        random_double(0.5, 1.0)
                    ),
                    static_cast<float>(
                        random_double(0.5, 1.0)
                    ),
                    static_cast<float>(
                        random_double(0.5, 1.0)
                    )
                );

                const float fuzz =
                    static_cast<float>(
                        random_double(0.0, 0.5)
                    );

                sphereMaterial =
                    scene.addMaterial(
                        MaterialType::Metal,
                        albedo,
                        fuzz
                    );
            }
            else
            {
                /*
                 * Glass. Albedo and fuzz are ignored by dielectric
                 * renderers, but must still be supplied before IOR
                 * in the current AddMaterial interface.
                 */
                sphereMaterial =
                    scene.addMaterial(
                        MaterialType::Dielectric,
                        glm::vec3(1.0f),
                        0.0f,
                        1.5f
                    );
            }

            scene.addSphere(
                center,
                0.2f,
                sphereMaterial
            );
        }
    }

    const MaterialId glassMaterial =scene.addMaterial(MaterialType::Dielectric,glm::vec3(1.0f),0.0f,1.5f);
    scene.addSphere(glm::vec3(0.0f, 1.0f, 0.0f),1.0f,glassMaterial);
    const MaterialId brownMaterial =scene.addMaterial(MaterialType::Lambertian,glm::vec3(0.4f, 0.2f, 0.1f));
    scene.addSphere(glm::vec3(-4.0f, 1.0f, 0.0f),1.0f,brownMaterial);
    const MaterialId metalMaterial = scene.addMaterial(MaterialType::Metal,glm::vec3(0.7f, 0.6f, 0.5f),0.0f);
    scene.addSphere(glm::vec3(4.0f, 1.0f, 0.0f),1.0f,metalMaterial);
    viewport(scene);

    SceneCamera sceneCamera;
    sceneCamera.lookFrom = glm::vec3(0.0);
    sceneCamera.lookAt = glm::vec3(0.0);
    sceneCamera.up = glm::vec3(0.0, 1.0, 0.0);
    sceneCamera.verticalFovDegrees = 20.0f;
    sceneCamera.defocusAngleDegrees = 4.0f;
    sceneCamera.focusDistance = 10.0f;
    scene.SetCamera(sceneCamera);

    world = BuildCpuWorld(scene);
    auto bvhRoot = std::make_shared<bvh_node>(world);
    world = hittable_list(bvhRoot);

    CPURenderer cpuRenderer;
    camera cam = cpuRenderer.cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 640;
    cam.samples_per_pixel = 50;
    cam.max_depth = 8;

    const SceneCamera& cameraData = scene.GetCamera();

    cam.vfov = cameraData.verticalFovDegrees;
    cam.lookfrom = point3(cameraData.lookFrom.x,cameraData.lookFrom.y,cameraData.lookFrom.z);
    cam.lookat = point3(cameraData.lookAt.x,cameraData.lookAt.y,cameraData.lookAt.z);
    cam.vup = vec3(cameraData.up.x,cameraData.up.y,cameraData.up.z);
    cam.defocus_angle = cameraData.defocusAngleDegrees;
    cam.focus_dist = cameraData.focusDistance;

    Renderer(scene);

    const auto start =std::chrono::steady_clock::now();
    // cam.render(world);
    const auto end =std::chrono::steady_clock::now();
    const auto elapsed =std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cerr
        << "\nRender time: "
        << elapsed.count()
        << " ms ("
        << elapsed.count() / 1000.0
        << " s)\n";
}

// void checkered_spheres(hittable_list& world, Scene& scene) {
//     auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));
//     scene.add(make_shared<sphere>(point3(0,-10, 0), 10, make_shared<lambertian>(checker)));
//
//     scene.add(make_shared<sphere>(point3(0, 10, 0), 10, make_shared<lambertian>(checker)));
//
//     for (const auto& object : scene.GetObjects())
//     {
//         world.add(object);
//     }
//     world = hittable_list(make_shared<bvh_node>(world));
//
//     EditorCamera editorCamera = viewport(scene);
//     CPURenderer my_cpu_renderer;
//     camera cam = my_cpu_renderer.cam;
//
//     cam.aspect_ratio      = 16.0 / 9.0;
//     cam.image_width       = 400;
//     cam.samples_per_pixel = 100;
//     cam.max_depth         = 50;
//
//     cam.vfov     = 20;
//     cam.lookfrom = point3(13,2,3);
//     cam.lookat   = point3(0,0,0);
//     cam.vup      = vec3(0,1,0);
//
//     cam.defocus_angle = 0;
//
//     cam.render(world);
// }

// void earth(hittable_list& world, Scene& scene) {
//     auto earth_texture = make_shared<image_texture>("external/earthmap.jpg");
//     auto earth_surface = make_shared<lambertian>(earth_texture);
//     scene.add(make_shared<sphere>(point3(0,0,0), 2, earth_surface));
//
//     for (const auto& object : scene.GetObjects())
//     {
//         world.add(object);
//     }
//     world = hittable_list(make_shared<bvh_node>(world));
//     EditorCamera editorCamera = viewport(scene);
//     CPURenderer my_cpu_renderer;
//     camera cam = my_cpu_renderer.cam;
//
//     cam.aspect_ratio      = 16.0 / 9.0;
//     cam.image_width       = 400;
//     cam.samples_per_pixel = 100;
//     cam.max_depth         = 50;
//
//     cam.vfov     = 20;
//     float focus = editorCamera.FocusDistance;
//     glm::vec3 target =
//         editorCamera.Position +
//         editorCamera.Front * focus;
//
//
//     cam.lookfrom = point3(
//         editorCamera.Position.x,
//         editorCamera.Position.y,
//         editorCamera.Position.z
//     );
//
//
//     cam.lookat = point3(
//         target.x,
//         target.y,
//         target.z
//     );
//
//     cam.vup = vec3(
//         editorCamera.Up.x,
//         editorCamera.Up.y,
//         editorCamera.Up.z
//     );
//
//     cam.defocus_angle = 0;
//
//     cam.render(world);
// }

// void perlin_spheres(hittable_list& world, Scene& scene) {
//
//     auto pertext = make_shared<noise_texture>(4);
//     scene.add(make_shared<sphere>(point3(0,-1000,0), 1000, make_shared<lambertian>(pertext)));
//     scene.add(make_shared<sphere>(point3(0,2,0), 2, make_shared<lambertian>(pertext)));
//
//     for (const auto& object : scene.GetObjects())
//     {
//         world.add(object);
//     }
//     world = hittable_list(make_shared<bvh_node>(world));
//     EditorCamera editorCamera = viewport(scene);
//     CPURenderer my_cpu_renderer;
//     camera cam = my_cpu_renderer.cam;
//
//     Renderer(scene);
//
//     cam.aspect_ratio      = 16.0 / 9.0;
//     cam.image_width       = 400;
//     cam.samples_per_pixel = 100;
//     cam.max_depth         = 50;
//
//     cam.vfov     = 20;
//     float focus = editorCamera.FocusDistance;
//     glm::vec3 target =
//         editorCamera.Position +
//         editorCamera.Front * focus;
//
//
//     cam.lookfrom = point3(
//         editorCamera.Position.x,
//         editorCamera.Position.y,
//         editorCamera.Position.z
//     );
//
//
//     cam.lookat = point3(
//         target.x,
//         target.y,
//         target.z
//     );
//
//     cam.vup = vec3(
//         editorCamera.Up.x,
//         editorCamera.Up.y,
//         editorCamera.Up.z
//     );
//
//     cam.defocus_angle = 0;
//
//     // cam.render(world);
// }

int main() {
    std::freopen("ImageOutputFiles/image.ppm", "w", stdout);

    // WORLD
    hittable_list world;
    Scene scene;
    switch (6) {
        case 1: bouncing_spheres(world, scene); break;
        // case 2: checkered_spheres(world, scene); break;
        // case 3: earth(world, scene); break;
        // case 4: perlin_spheres(world, scene); break;
        case 5: quads(world, scene); break;
        case 6: cornell(world, scene); break;
    }
}
