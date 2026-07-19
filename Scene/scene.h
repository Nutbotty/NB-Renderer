//
// Created by Nutbotty on 7/17/2026.
//

#ifndef NB_RENDERER_SCENE_H
#define NB_RENDERER_SCENE_H

#include <memory>
#include <vector>

#include "../hittable.h"
#include "../nbrenderer.h"

class Scene
{
public:
    Scene() = default;

    void add(const std::shared_ptr<hittable>& object)
    {
        m_Objects.push_back(object);
        m_Dirty = true;
    }

    void Clear()
    {
        m_Objects.clear();
        m_Dirty = true;
    }

    const std::vector<std::shared_ptr<hittable>>& GetObjects() const
    {
        return m_Objects;
    }

    std::vector<std::shared_ptr<hittable>>& GetObjects()
    {
        return m_Objects;
    }

    bool IsDirty() const
    {
        return m_Dirty;
    }

    void ClearDirty()
    {
        m_Dirty = false;
    }

private:
    std::vector<std::shared_ptr<hittable>> m_Objects;

    bool m_Dirty = true;
};

#endif //NB_RENDERER_SCENE_H
