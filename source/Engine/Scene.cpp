// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Scene.h"

#include <algorithm>

namespace Sandbox3D::Engine
{
    Scene::Scene(std::string name)
        : m_name(std::move(name))
    {
    }

    void Scene::AddObject(std::shared_ptr<Base> object)
    {
        if (!object)
        {
            return;
        }

        m_objects.push_back(object);
        if (auto body = std::dynamic_pointer_cast<Body>(object))
        {
            if (std::find(m_bodies.begin(), m_bodies.end(), body) == m_bodies.end())
            {
                m_bodies.push_back(std::move(body));
            }
        }
        if (auto terrain = std::dynamic_pointer_cast<Terrain::TerrainObject>(object))
        {
            if (std::find(m_terrains.begin(), m_terrains.end(), terrain) == m_terrains.end())
            {
                m_terrains.push_back(std::move(terrain));
            }
        }
        if (auto light = std::dynamic_pointer_cast<Light>(object))
        {
            if (std::find(m_lights.begin(), m_lights.end(), light) == m_lights.end())
            {
                m_lights.push_back(std::move(light));
            }
        }
    }

    void Scene::AddBody(std::shared_ptr<Body> body)
    {
        if (body)
        {
            AddObject(std::move(body));
        }
    }

    void Scene::AddTerrain(std::shared_ptr<Terrain::TerrainObject> terrain)
    {
        if (terrain)
        {
            AddObject(std::move(terrain));
        }
    }

    void Scene::AddLight(std::shared_ptr<Light> light)
    {
        if (light)
        {
            AddObject(std::move(light));
        }
    }

    void Scene::RemoveObject(std::string_view name)
    {
        std::erase_if(m_objects, [name](const std::shared_ptr<Base>& obj) {
            return obj && obj->GetName() == name;
        });
        std::erase_if(m_bodies, [name](const std::shared_ptr<Body>& body) {
            return body && body->GetName() == name;
        });
        std::erase_if(m_terrains, [name](const std::shared_ptr<Terrain::TerrainObject>& terrain) {
            return terrain && terrain->GetName() == name;
        });
        std::erase_if(m_lights, [name](const std::shared_ptr<Light>& light) {
            return light && light->GetName() == name;
        });
        if (m_terrain && m_terrain->GetName() == name)
        {
            m_terrain.reset();
        }
        if (m_sunLight && m_sunLight->GetName() == name)
        {
            m_sunLight.reset();
        }
        if (m_earthBounceLight && m_earthBounceLight->GetName() == name)
        {
            m_earthBounceLight.reset();
        }
    }

    void Scene::RemoveObject(uint32_t id)
    {
        std::vector<uint32_t> idsToRemove;
        std::vector<std::shared_ptr<Base>> stack;
        if (auto root = FindObject(id))
        {
            stack.push_back(root);
            while (!stack.empty())
            {
                auto current = std::move(stack.back());
                stack.pop_back();
                idsToRemove.push_back(current->GetId());
                for (const auto& child : current->GetChildren())
                {
                    if (child)
                    {
                        stack.push_back(child);
                    }
                }
            }
            Base::SetParent(root, nullptr);
        }
        else
        {
            idsToRemove.push_back(id);
        }

        const auto shouldRemove = [&idsToRemove](uint32_t objectId) {
            return std::find(idsToRemove.begin(), idsToRemove.end(), objectId) != idsToRemove.end();
        };

        std::erase_if(m_objects, [&shouldRemove](const std::shared_ptr<Base>& obj) {
            return obj && shouldRemove(obj->GetId());
        });
        std::erase_if(m_bodies, [&shouldRemove](const std::shared_ptr<Body>& body) {
            return body && shouldRemove(body->GetId());
        });
        std::erase_if(m_terrains, [&shouldRemove](const std::shared_ptr<Terrain::TerrainObject>& terrain) {
            return terrain && shouldRemove(terrain->GetId());
        });
        std::erase_if(m_lights, [&shouldRemove](const std::shared_ptr<Light>& light) {
            return light && shouldRemove(light->GetId());
        });
        if (m_terrain && shouldRemove(m_terrain->GetId()))
        {
            m_terrain.reset();
        }
        if (m_sunLight && shouldRemove(m_sunLight->GetId()))
        {
            m_sunLight.reset();
        }
        if (m_earthBounceLight && shouldRemove(m_earthBounceLight->GetId()))
        {
            m_earthBounceLight.reset();
        }
    }

    void Scene::RemoveBody(std::string_view name)
    {
        RemoveObject(name);
    }

    void Scene::RemoveBody(uint32_t id)
    {
        RemoveObject(id);
    }

    void Scene::RemoveTerrain(std::string_view name)
    {
        RemoveObject(name);
    }

    void Scene::RemoveTerrain(uint32_t id)
    {
        RemoveObject(id);
    }

    void Scene::RemoveLight(std::string_view name)
    {
        RemoveObject(name);
    }

    void Scene::RemoveLight(uint32_t id)
    {
        RemoveObject(id);
    }

    std::shared_ptr<Base> Scene::FindObject(std::string_view name) const noexcept
    {
        for (const auto& obj : m_objects)
        {
            if (obj && obj->GetName() == name)
            {
                return obj;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Base> Scene::FindObject(uint32_t id) const noexcept
    {
        for (const auto& obj : m_objects)
        {
            if (obj && obj->GetId() == id)
            {
                return obj;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Body> Scene::FindBody(std::string_view name) const noexcept
    {
        for (const auto& body : m_bodies)
        {
            if (body && body->GetName() == name)
            {
                return body;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Terrain::TerrainObject> Scene::FindTerrain(std::string_view name) const noexcept
    {
        for (const auto& terrain : m_terrains)
        {
            if (terrain && terrain->GetName() == name)
            {
                return terrain;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Light> Scene::FindLight(std::string_view name) const noexcept
    {
        for (const auto& light : m_lights)
        {
            if (light && light->GetName() == name)
            {
                return light;
            }
        }
        return nullptr;
    }

    void Scene::AddRenderItem(Renderer::RenderItem item)
    {
        m_renderItems.push_back(std::move(item));
    }

    void Scene::AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix, const std::string& name)
    {
        m_renderItems.push_back(Renderer::RenderItem{ std::move(mesh), worldMatrix, true, name });
    }

    void Scene::AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Renderer::Material> material, const Maths::Mat4x4D& worldMatrix, const std::string& name)
    {
        m_renderItems.push_back(Renderer::RenderItem{ std::move(mesh), std::move(material), worldMatrix, true, name });
    }

    void Scene::RemoveRenderItem(std::string_view name)
    {
        std::erase_if(m_renderItems, [name](const Renderer::RenderItem& item) { return item.name == name; });
    }

    void Scene::ClearRenderItems() noexcept
    {
        m_renderItems.clear();
    }

    void Scene::CollectRenderItems(std::vector<Renderer::RenderItem>& outItems) const
    {
        for (const auto& obj : m_objects)
        {
            if (obj && obj->IsActive() && obj->IsRenderable() && obj->IsVisible())
            {
                for (const auto& item : obj->GetRenderItems())
                {
                    if (item.mesh && item.isVisible)
                    {
                        outItems.push_back(item);
                    }
                }
            }
        }

        for (const auto& item : m_renderItems)
        {
            if (item.mesh && item.isVisible)
            {
                outItems.push_back(item);
            }
        }
    }

    std::span<const Renderer::RenderItem> Scene::GetRenderItems() const noexcept
    {
        m_cachedRenderItems.clear();
        CollectRenderItems(m_cachedRenderItems);
        return m_cachedRenderItems;
    }

    std::span<const Renderer::GpuLight> Scene::GetLightData(const Maths::Vec3D& cameraPosition) const noexcept
    {
        m_cachedGpuLights.clear();

        for (const auto& light : m_lights)
        {
            if (light && light->IsActive() && light->IsVisible())
            {
                if (m_cachedGpuLights.size() < Renderer::MaxLights)
                {
                    m_cachedGpuLights.push_back(light->ToGpuLight(cameraPosition));
                }
            }
        }

        return m_cachedGpuLights;
    }

    Renderer::RenderItem* Scene::FindRenderItem(std::string_view name) noexcept
    {
        for (auto& item : m_renderItems)
        {
            if (item.name == name)
            {
                return &item;
            }
        }
        return nullptr;
    }

    const Renderer::RenderItem* Scene::FindRenderItem(std::string_view name) const noexcept
    {
        for (const auto& item : m_renderItems)
        {
            if (item.name == name)
            {
                return &item;
            }
        }
        return nullptr;
    }

    void Scene::UpdateObjects(const UpdateContext& context)
    {
        for (const auto& object : m_objects)
        {
            if (object && object->IsActive())
            {
                object->Update(context);
            }
        }
    }
}
