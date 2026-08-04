//
// Created by Nutbotty on 8/3/2026.
//

#ifndef NB_RENDERER_MODELLOADER_H
#define NB_RENDERER_MODELLOADER_H

#pragma once

#include <filesystem>

class Scene;

class GltfLoader {
public:
    [[nodiscard]]
    static bool Load(const std::filesystem::path& path, Scene& scene);
};

#endif //NB_RENDERER_MODELLOADER_H
