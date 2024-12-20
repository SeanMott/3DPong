/*
Test rendering project Pong remade in 3D with the new Bytes The Dust Standard Library (3rd edition) and new Smok renderer.

- custom mesh loading
- GUI rendering
- mesh rendering
- local player input from multable controllers
*/

#include <3DPong/Engine.hpp>
#include <3DPong/Renderer/FrameManager.hpp>
#include <3DPong/Renderer/MeshRenderer.hpp>
#include <3DPong/ECS/Scene.hpp>

#include <BTDSTD/Time.hpp>
#include <BTDSTD/Input/KeyInput.hpp>

#include <BTDSTD/Wireframe/Core/GPU.hpp>
#include <BTDSTD/Wireframe/Core/DesktopSwapchain.hpp>
#include <BTDSTD/Wireframe/Core/DesktopWindow.hpp>
		 
#include <BTDSTD/Wireframe/Renderpass.hpp>
		 
#include <BTDSTD/Wireframe/CommandBuffer.hpp>
#include <BTDSTD/Wireframe/FrameBuffer.hpp>
#include <BTDSTD/Wireframe/SyncObjects.hpp>

#include <TyGUI/WidgetRenderer.hpp>

#include <Smok/Assets/AssetManager.hpp>
#include <Smok/Assets/Mesh.hpp>

#include <Smok/Components/Transform.hpp>
#include <Smok/Components/Camera.hpp>
#include <Smok/Components/MeshComponent.hpp>

#include <Smok/Memory/LifetimeDeleteQueue.hpp>

#include <SDL_events.h>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <chrono>
#include <thread>
#include <vector>

void init_pipelines(
	Smok::Asset::AssetManager::Asset_PipelineLayout& pipelineLayout, Smok::Asset::AssetManager::Asset_GraphicsPipeline& pipeline,
	Wireframe::Renderpass::Renderpass& renderpass,
	Pong3D::Core::Engine* engine)
{
	Wireframe::Device::GPU* GPU = &engine->GPU;

	//loads a pipeline settings
	Wireframe::Pipeline::PipelineSettings pipelineSettings;
	Wireframe::Pipeline::Serilize::LoadPipelineSettingsDataFromFile(pipeline.pipelineDataSettingFile, pipelineSettings);

	//sets the vertex layout
	Wireframe::Pipeline::VertexInputDescription vertexDescription = Smok::Asset::Mesh::Vertex::GenerateVertexInputDescription();
	pipelineSettings._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineSettings._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipelineSettings._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineSettings._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	//loads the shaders
	Wireframe::Shader::ShaderModule meshVertShader;
	Wireframe::Shader::Serilize::ShaderSerilizeData vertex;
	Wireframe::Shader::Serilize::LoadShaderDataFromFile(pipeline.vertexShaderDataSettingFile,
		vertex, false);
	if (!meshVertShader.Create(vertex.binaryFilepath.c_str(), GPU)) {}
	
	Wireframe::Shader::ShaderModule meshFragShader;
	Wireframe::Shader::Serilize::ShaderSerilizeData fragment;
	Wireframe::Shader::Serilize::LoadShaderDataFromFile(pipeline.fragmentShaderDataSettingFile,
		fragment, false);
	if (!meshFragShader.Create(fragment.binaryFilepath.c_str(), GPU)) {}
	
	pipelineSettings._shaderStages = { Wireframe::Shader::GenerateShaderStageInfoForPipeline(meshVertShader, Wireframe::Shader::Util::ShaderStage::Vertex),
		Wireframe::Shader::GenerateShaderStageInfoForPipeline(meshFragShader, Wireframe::Shader::Util::ShaderStage::Fragment) };

	//generates a layout and the push constants
	Wireframe::Pipeline::PipelineLayout_CreateInfo pipelineLayoutInfo;
	Wireframe::Pipeline::PushConstant p;
	Wireframe::Pipeline::Serilize::LoadPipelineLayoutPushConstantDataFromFile(pipelineLayout.pushConstantDataSettingFile,
		p);
	p.size = sizeof(Pong3D::Renderer::MeshPushConstants);
	pipelineLayoutInfo.pushConstants.emplace_back(p);

	pipelineLayout.asset.Create(pipelineLayoutInfo, GPU);
	pipelineLayout.assetIsCreated = true;
	pipeline.asset.Create(pipelineSettings, pipelineLayout.asset, renderpass._renderPass, GPU);
	pipeline.assetIsCreated = true;

	meshFragShader.Destroy(GPU);
	meshVertShader.Destroy(GPU);
}

//defines a input compoent for Pong Bars
struct PlayerInputComponent : public BTD::ECS::Comp::IComponent
{
	//the up and down key
	SDL_Scancode upKey = SDL_SCANCODE_W, downKey = SDL_SCANCODE_S;
};

//defines a editor camera input component, used in debug

//defines a network component

//entry point
int main()
{
	//--init

	//initalize the engine and create a window
	Pong3D::Core::Engine engine;
	if (!engine.Init())
	{
		engine.Shutdown();
		getchar();
		return -1;
	}
	Wireframe::Window::DesktopWindow* window = &engine.window;

	//initalize render stuff
	Pong3D::Renderer::FrameRenderManager renderManager;
	renderManager.Init(&engine);
	renderManager.TyGUI_Init();

	//registers assets
	Smok::Asset::AssetManager::AssetManager AM;
	uint64_t meshPipelineAssetID = AM.RegisterAsset_GraphicsPipeline("meshPipeline_Default",
		BTD::IO::FileInfo("Pipelines/meshSettings." + Wireframe::Pipeline::Serilize::GetPipelineSettingExtentionStr()),
		BTD::IO::FileInfo("shaders/mesh_vertex." + Wireframe::Shader::Serilize::ShaderSerilizeData::GetExtentionStr()),
		BTD::IO::FileInfo("shaders/mesh_fragment." + Wireframe::Shader::Serilize::ShaderSerilizeData::GetExtentionStr())),
		
		meshPipelineLayoutAssetID = AM.RegisterAsset_PipelineLayout("meshPipelineLayout_Default",
			BTD::IO::FileInfo("Pipelines/meshPushConstant." + Wireframe::Pipeline::PushConstant::GetExtentionStr())),
		
		staticMeshAssetID = AM.RegisterAsset_StaticMesh("staticMesh_Default",
			BTD::IO::FileInfo("assets/Guitar." + Smok::Asset::Mesh::Serilize::GetSmeshDeclFileExtensionStr()),
			BTD::IO::FileInfo("assets/Guitar." + Smok::Asset::Mesh::Serilize::GetSmeshBinaryFileExtensionStr()));

	//loads the basic pipeline
	init_pipelines(AM.pipelineLayouts[meshPipelineLayoutAssetID], AM.pipelines[meshPipelineAssetID], renderManager.renderpass, &engine);

	//load static mesh
	AM.staticMeshes[staticMeshAssetID].LoadMesh();
	AM.staticMeshes[staticMeshAssetID].InitalizeMesh(engine._allocator);

	//----scene
	Pong3D::Scene::Scene scene;

	//camera
	Smok::ECS::Comp::Transform transform; Smok::ECS::Comp::Camera cameraSettings;
	transform.position = { 0.f, 0.f, -10.f };
	Pong3D::Scene::Entity camera = scene.Camera_Create("Main Camera", transform, cameraSettings, &engine);

	//entity paddle P1
	Smok::ECS::Comp::MeshRender entity_meshRenderComp;
	entity_meshRenderComp.pipelineID = meshPipelineAssetID;
	entity_meshRenderComp.pipelineLayoutID = meshPipelineLayoutAssetID;
	entity_meshRenderComp.staticMeshID = staticMeshAssetID;

	transform.position = { 0.0f, 0.0f, 7.0f };

	scene.CreateEntity_Paddle("Paddle 1", transform, entity_meshRenderComp, &engine);

	//entity paddle P2
	entity_meshRenderComp = Smok::ECS::Comp::MeshRender();
	entity_meshRenderComp.pipelineID = meshPipelineAssetID;
	entity_meshRenderComp.pipelineLayoutID = meshPipelineLayoutAssetID;
	entity_meshRenderComp.staticMeshID = staticMeshAssetID;

	transform.position = { 0.0f, 5.0f, 7.0f };

	scene.CreateEntity_Paddle("Paddle 2", transform, entity_meshRenderComp, &engine);

	//goes through the scene and generate the render operations
	std::deque<Pong3D::Renderer::RenderOperationBatch> renderOperationBatchs;
	Pong3D::Renderer::RenderOperationBatch* batch = &renderOperationBatchs.emplace_back(Pong3D::Renderer::RenderOperationBatch());

	auto comps = BTD::ECS::queryEntities<Smok::ECS::Comp::Transform, Smok::ECS::Comp::MeshRender>();
	for (size_t i = 0; i < comps.size(); ++i)
	{
		transform = *BTD::ECS::getComponent<Smok::ECS::Comp::Transform>(comps[i]);
		Smok::ECS::Comp::MeshRender mr = *BTD::ECS::getComponent<Smok::ECS::Comp::MeshRender>(comps[i]);
		batch->AddStaticMesh(&AM.pipelineLayouts[mr.pipelineLayoutID].asset, &AM.pipelines[mr.pipelineID].asset,
			&AM.staticMeshes[mr.staticMeshID].asset, transform);
	}

	//gets the main camera
	Smok::ECS::Comp::Transform* camTrans = BTD::ECS::getComponent<Smok::ECS::Comp::Transform>(scene.cameras[0].ID);
	Smok::ECS::Comp::Camera* cam = BTD::ECS::getComponent<Smok::ECS::Comp::Camera>(scene.cameras[0].ID);

	//--game loop
	BTD::Time::Time time(60.0f);
	SDL_Event e;
	bool bQuit = false;
	bool stop_rendering = false;
	BTD::Input::KeyInput keyInputData;
	while (window->isRunning)
	{
		//--cal deltas
		time.Update();

		//--input

		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;

			//window minimize or unminimize
			if (e.type == SDL_WINDOWEVENT) {

				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					window->isMinimized = true;
					stop_rendering = true;
				}

				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					window->isMinimized = false;
					stop_rendering = false;
				}
			}

			//sends input to the widgets
			if (renderManager.TyGUIIsInitalized && renderManager.TyGUIWidgetsShouldRender)
				renderManager.widgetRenderer.SendInputData(&e);
		}

		if (bQuit) //quits app
		{
			window->isRunning = false;
			break;
		}

		keyInputData.UpdateInputData();

		//gets all input components and their trans components

		//input for moving the camera
		if (keyInputData.IsKeyHeld(SDL_SCANCODE_W))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Forward() * (-15.0f * time.GetFixedDeltaTime());
		}

		else if (keyInputData.IsKeyHeld(SDL_SCANCODE_S))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Forward() * (15.0f * time.GetFixedDeltaTime());
		}

		else if (keyInputData.IsKeyHeld(SDL_SCANCODE_A))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Left() * (15.0f * time.GetFixedDeltaTime());
		}

		else if (keyInputData.IsKeyHeld(SDL_SCANCODE_D))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Left() * (-15.0f * time.GetFixedDeltaTime());
		}

		//--update

		while (time.ShouldUpdate()) {
			// Perform fixed-timestep updates here (e.g., physics)
			time.ConsumeAccumulator();
		}

		//--render

		//do not draw if we are minimized
		if (stop_rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		//handles TyGUI widgets
		if (renderManager.TyGUIIsInitalized && renderManager.TyGUIWidgetsShouldRender)
		{
			renderManager.widgetRenderer.StartFrame();

			ImGui::Begin("OWO");

			ImGui::End();
		}

		//updates the cameras if they're bound to the screen
		cam->renderSize = { (float)engine.window._windowExtent.width, (float)engine.window._windowExtent.height };

		//starts the frame
		Pong3D::Renderer::Frame frame = renderManager.StartFrame();

		//performs renders
		for (size_t i = 0; i < renderOperationBatchs.size(); ++i)
			renderOperationBatchs[i].PerformRender(frame.cmd.handle, camTrans, cam);

		//submits the frame
		renderManager.SubmitFrame(frame);
	}
	window = nullptr;
	vkDeviceWaitIdle(engine.GPU.device); //make sure the gpu has stopped doing its things

	//--clean up
	AM.Destroy(engine._allocator, &engine.GPU);
	renderManager.Shutdown();
	engine.Shutdown();

	getchar();
	return 0;
}