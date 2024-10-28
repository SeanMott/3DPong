#pragma once

//defines a mesh renderer

#include <BTDSTD/Wireframe/Pipeline/GraphicsPipeline.hpp>

#include <Smok/Assets/Mesh.hpp>
#include <Smok/Components/Camera.hpp>
#include <Smok/Components/Transform.hpp>

#include <deque>

namespace Pong3D::Renderer
{
	struct MeshPushConstants
	{
		glm::vec4 color = glm::vec4(200.0f, 0.0f, 215.0f, 255.0f);
		glm::mat4 render_matrix = glm::mat4(1.0f);
	};

	//defines a static mesh render operation
	struct RenderOperation_StaticMesh
	{
		uint64_t meshPipelineLayoutIndex = 0, meshPipelineIndex, meshIndex;

		Smok::ECS::Comp::Transform entityTransform;
	};

	//defines a batch
	struct RenderOperationBatch
	{
		//asstes
		std::deque<Wireframe::Pipeline::PipelineLayout*> pipelineLayouts;
		std::deque<Wireframe::Pipeline::GraphicsPipeline*> pipelines;
		std::deque<Smok::Asset::Mesh::StaticMesh*> staticMeshes;

		//render operations
		std::deque<RenderOperation_StaticMesh> renderOperations_staticMesh;

		//adds a static mesh
		inline RenderOperation_StaticMesh* AddStaticMesh(Wireframe::Pipeline::PipelineLayout* layout, Wireframe::Pipeline::GraphicsPipeline* pipeline, Smok::Asset::Mesh::StaticMesh* mesh)
		{
			RenderOperation_StaticMesh* op = &renderOperations_staticMesh.emplace_back(RenderOperation_StaticMesh());

			//gets the pipeline layout asset
			uint64_t index = 0; bool found = false;
			for (uint32_t i = 0; pipelineLayouts.size(); ++i)
			{
				if (pipelineLayouts[i] == layout)
				{
					found = true;
					index = i;
					break;
				}
			}
			if (!found)
			{
				pipelineLayouts.emplace_back(layout);
				index = pipelineLayouts.size() - 1;
			}
			op->meshPipelineLayoutIndex = index;

			//gets pipeline asset
			index = 0; found = false;
			for (uint32_t i = 0; pipelines.size(); ++i)
			{
				if (pipelines[i] == pipeline)
				{
					found = true;
					index = i;
					break;
				}
			}
			if (!found)
			{
				pipelines.emplace_back(pipeline);
				index = pipelines.size() - 1;
			}
			op->meshPipelineIndex = index;

			//gets static mesh asset
			index = 0; found = false;
			for (uint32_t i = 0; staticMeshes.size(); ++i)
			{
				if (staticMeshes[i] == mesh)
				{
					found = true;
					index = i;
					break;
				}
			}
			if (!found)
			{
				staticMeshes.emplace_back(mesh);
				index = staticMeshes.size() - 1;
			}
			op->meshIndex = index;

			return op;
		}

		//adds a static mesh
		inline RenderOperation_StaticMesh* AddStaticMesh(Wireframe::Pipeline::PipelineLayout* layout, Wireframe::Pipeline::GraphicsPipeline* pipeline, Smok::Asset::Mesh::StaticMesh* mesh,
			const Smok::ECS::Comp::Transform& entityTransform)
		{
			RenderOperation_StaticMesh* op = AddStaticMesh(layout, pipeline, mesh);

			op->entityTransform = entityTransform;

			return op;
		}

		//perform operations
		inline void PerformRender(VkCommandBuffer& cmd, Smok::ECS::Comp::Transform* cameraTransform, Smok::ECS::Comp::Camera* cameraSettings)
		{
			for (size_t i = 0; i < renderOperations_staticMesh.size(); ++i)
			{
				//binds the pipeline if it isn't already
				pipelines[renderOperations_staticMesh[i].meshPipelineIndex]->Bind(cmd);

				//updates any pipelines bound to the window size
				VkViewport v = {};
				v.width = cameraSettings->renderSize.x;
				v.height = cameraSettings->renderSize.y;
				VkRect2D s = {};
				s.extent = { (uint32_t)v.width, (uint32_t)v.height };
				pipelines[renderOperations_staticMesh[i].meshPipelineIndex]->SetViewport(cmd, v);
				pipelines[renderOperations_staticMesh[i].meshPipelineIndex]->SetScissor(cmd, s);

				//pushes data
				MeshPushConstants data;
				data.render_matrix = cameraSettings->GeneratePerspective() * cameraSettings->GenerateView(cameraTransform->position) * renderOperations_staticMesh[i].entityTransform.ModelMatrix();
				pipelineLayouts[renderOperations_staticMesh[i].meshPipelineLayoutIndex]->UpdatePushConstant_Vertex(cmd, "Mesh Data", &data);

				//binds and draws meshes
				for (size_t m = 0; m < staticMeshes[renderOperations_staticMesh[i].meshIndex]->meshes.size(); ++m)
				{
					staticMeshes[i]->meshes[m].vertexBuffer.Bind(cmd);
					staticMeshes[i]->meshes[m].indexBuffer.Bind(cmd);
					staticMeshes[i]->meshes[m].indexBuffer.Draw(cmd, 1, 0);
				}
			}
		}
	};
}