#pragma once

//defines a scene since scenes are not structures in Smok, every game will define their own that generates the render data

#include <BTDSTD/Maps/StringIDRegistery.hpp>

#include <3DPong/Engine.hpp>

#include <Smok/Components/MeshComponent.hpp>
#include <Smok/Components/Transform.hpp>

namespace Pong3D::Scene
{
	//defines a flag for differant entity types
	enum class EntityFlag
	{
		Camera = 0, //defines a camera

		Rider, //defines a Rider

		Item, //defines a Item

		Envirment, //defines the envirment

		Count
	};

	//defines a entity
	struct Entity
	{
		uint64_t ID = 0; //the ID associated with this entity
	
		EntityFlag flag = EntityFlag::Count; //defines the flag for entity type
	};

	//defines a scene
	struct Scene
	{
		bool isDirty = true; //has the scene itself changed

		BTD::Map::IDStringRegistery registery; //the entity register, mapping a string name to a entity ID

		std::vector<Entity> cameras; //the cameras, normally only one, but others are kept for the other riders for hot swapping perspectives

		std::vector<Entity> entities; //the entities

		Scene()
		{
			cameras.reserve(2);
			entities.reserve(10);
		}

		//creates a camera
		inline Entity Camera_Create(const std::string& name, const Smok::ECS::Comp::Transform& transform, const Smok::ECS::Comp::Camera& cameraSettings, Core::Engine* engine)
		{
			//creates ID
			Entity cam;
			cam.ID = registery.GenerateID(name);
			cam.flag = EntityFlag::Camera;
		
			//creates components
			BTD::ECS::addComponent(cam.ID, transform);
			BTD::ECS::addComponent(cam.ID, cameraSettings);

			//adds to world
			isDirty = true;
			return cameras.emplace_back(cam);
		}

		//deletes the main camera

		//gets the main camera

		//creates a Rider
		inline Entity Rider_Create(const std::string& name, const Smok::ECS::Comp::Transform& transform, const Smok::ECS::Comp::MeshRender& meshRenderer, Core::Engine* engine)
		{
			//creates ID
			Entity rider;
			rider.ID = registery.GenerateID(name);
			rider.flag = EntityFlag::Rider;

			//creates components
			BTD::ECS::addComponent(rider.ID, transform);
			BTD::ECS::addComponent(rider.ID, meshRenderer);

			//adds to world
			isDirty = true;
			return entities.emplace_back(rider);
		}

		//returns all the IDs of entities
		inline std::vector<uint64_t> GetEntitiesAsIDs()
		{
			std::vector<uint64_t> entityIDs; const size_t entityCount = entities.size(); entityIDs.resize(entityCount);
			for (size_t i = 0; i < entityCount; ++i)
				entityIDs[i] = entities[i].ID;

			return entityIDs;
		}
	};
}