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
		glm::mat4 render_matrix = glm::mat4(1.0f);
		alignas(16) glm::vec4 color = glm::vec4(200.0f, 0.0f, 215.0f, 255.0f);
	};

	//defines a draw call
	struct DrawCallOp
	{
		glm::mat4 modelMatrix = glm::mat4(1.0f);
	};

	//defines a static mesh render operation
	struct RenderOperation_StaticMesh
	{
		uint64_t meshPipelineLayoutIndex = 0, meshPipelineIndex, meshIndex;

		std::vector<DrawCallOp> ops;
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
		inline RenderOperation_StaticMesh* AddStaticMesh(Wireframe::Pipeline::PipelineLayout* layout, Wireframe::Pipeline::GraphicsPipeline* pipeline, Smok::Asset::Mesh::StaticMesh* mesh,
			Smok::ECS::Comp::Transform& entityTransform)
		{
			RenderOperation_StaticMesh* op = &renderOperations_staticMesh.emplace_back(RenderOperation_StaticMesh());

			//checks if batches already exist containing the desired assets
			pipelineLayouts.emplace_back(layout);
			op->meshPipelineLayoutIndex = pipelineLayouts.size() - 1;

			pipelines.emplace_back(pipeline);
			op->meshPipelineIndex = pipelines.size() - 1;

			staticMeshes.emplace_back(mesh);
			op->meshIndex = staticMeshes.size() - 1;

			//adds a draw call
			DrawCallOp dc;
			dc.modelMatrix = entityTransform.ModelMatrix();
			op->ops.emplace_back(dc);

			return op;
		}

		//perform operations
		inline void PerformRender(VkCommandBuffer& cmd, Smok::ECS::Comp::Transform* cameraTransform, Smok::ECS::Comp::Camera* cameraSettings)
		{
			Wireframe::Pipeline::GraphicsPipeline* lastPipeline = nullptr;
			Smok::Asset::Mesh::StaticMesh* lastStaticMesh = nullptr;
			for (size_t i = 0; i < renderOperations_staticMesh.size(); ++i)
			{
				//binds the pipeline if it isn't already
				if (lastPipeline != pipelines[renderOperations_staticMesh[i].meshPipelineIndex])
				{
					lastPipeline = pipelines[renderOperations_staticMesh[i].meshPipelineIndex];
					lastPipeline->Bind(cmd);
				}

				//updates any pipelines bound to the window size
				VkViewport v = {};
				v.width = cameraSettings->renderSize.x;
				v.height = cameraSettings->renderSize.y;
				VkRect2D s = {};
				s.extent = { (uint32_t)v.width, (uint32_t)v.height };
				lastPipeline->SetViewport(cmd, v);
				lastPipeline->SetScissor(cmd, s);

				//binds the mesh if it's not
				if (lastStaticMesh != staticMeshes[renderOperations_staticMesh[i].meshIndex])
				{
					lastStaticMesh = staticMeshes[renderOperations_staticMesh[i].meshIndex];
					lastStaticMesh->vertexBuffer.Bind(cmd);
				}

				//calculate the PV
				const glm::mat4 PV = cameraSettings->GeneratePerspective() * cameraSettings->GenerateView(cameraTransform->position);

				//pushes data
				MeshPushConstants data;
				data.render_matrix = PV * renderOperations_staticMesh[i].ops[0].modelMatrix;
				pipelineLayouts[renderOperations_staticMesh[i].meshPipelineLayoutIndex]->UpdatePushConstant_Vertex(cmd, "Mesh Data", &data);

				//draws meshes
				for (size_t m = 0; m < lastStaticMesh->meshes.size(); ++m)
				{
					lastStaticMesh->meshes[m].indexBuffer.Bind(cmd);
					lastStaticMesh->meshes[m].indexBuffer.Draw(cmd, 1, 0);
				}
			}
		}
	};
}