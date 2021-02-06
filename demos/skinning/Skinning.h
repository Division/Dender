﻿#pragma once

#include <vector>
#include "Engine.h"
#include "ecs/ECS.h"
#include "ecs/TransformGraph.h"
#include "ecs/systems/TransformSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/components/Light.h"
#include "ecs/components/Physics.h"
#include "resources/ResourceCache.h"
#include "resources/MaterialResource.h"


class Mesh;
class Material;
class ViewerCamera;

namespace render
{
	class MaterialList;
}

namespace Vehicle::Utils
{
	class VehicleDataCache;
}

namespace ECS::components
{
	struct DeltaTime;
}

namespace game
{
	class Gameplay;
}

namespace Resources
{
	class TextureResource;
	class MultiMesh;
	class PhysCollider;
	class MaterialResource;
	class EntityResource;
	class SkeletonResource;
	class SkeletalAnimationResource;
}

class Game : public IGame, public IGamePhysicsDelegate {
public:
	Game();
	~Game();
	void init() override;
	void update(float dt) override;
	void UpdatePhysics(float dt) override;
	IGamePhysicsDelegate* GetPhysicsDelegate() override;
	physx::PxSimulationFilterShader GetFilterShader() override;
	physx::PxVec3 GetGravity() override;
	void cleanup() override;

private:
	std::unique_ptr<ViewerCamera> camera;

	ECS::EntityManager* manager = nullptr;
	ECS::TransformGraph* graph = nullptr;

	Resources::Handle<Resources::EntityResource> animated_entity;
	ECS::EntityID animated_entity_id;

	Resources::Handle<Resources::SkeletalAnimationResource> animation;

	bool camera_control = false;
};
