#include "pch.h"
#include "AssetManager.h"

#include <ResourceUploadBatch.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static AssetManager* g_pAssetManager = nullptr;

void AssetManager::Initialize()
{
	if (!g_pAssetManager)
	{
		g_pAssetManager = new AssetManager();
	}
}

void AssetManager::Shutdown()
{
	if (g_pAssetManager)
	{
		delete g_pAssetManager;
	}
}

AssetManager& AssetManager::Instance()
{
	assert(g_pAssetManager);
	return *g_pAssetManager;
}

AssetManager::AssetManager()
{
	m_AsyncImageLoader.SetCallback([&](auto pImage)
	{
		std::scoped_lock _(m_UploadCriticalSection);

		m_ImageUploadQueue.push(std::move(pImage));
		m_UploadConditionVariable.Wake();
	});

	m_AsyncMeshLoader.SetCallback([&](auto pMesh)
	{
		std::scoped_lock _(m_UploadCriticalSection);

		m_MeshUploadQueue.push(std::move(pMesh));
		m_UploadConditionVariable.Wake();
	});

	m_Thread.reset(::CreateThread(nullptr, 0, &ResourceUploadThreadProc, nullptr, 0, nullptr));
}

AssetManager::~AssetManager()
{
	m_ShutdownThread = true;
	m_UploadConditionVariable.WakeAll();

	::WaitForSingleObject(m_Thread.get(), INFINITE);
}

void AssetManager::AsyncLoadImage(const std::filesystem::path& Path, bool sRGB)
{
	if (!std::filesystem::exists(Path))
	{
		return;
	}

	entt::id_type hs = entt::hashed_string(Path.string().data());
	if (m_ImageCache.Exist(hs))
	{
		LOG_INFO("{} Exists", Path.string());
		return;
	}

	Asset::ImageMetadata metadata = {
		.Path = Path,
		.sRGB = sRGB };
	m_AsyncImageLoader.RequestAsyncLoad(1, &metadata);
}

void AssetManager::AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM)
{
	if (!std::filesystem::exists(Path))
	{
		return;
	}

	entt::id_type hs = entt::hashed_string(Path.string().data());
	if (m_MeshCache.Exist(hs))
	{
		LOG_INFO("{} Exists", Path.string());
		return;
	}

	Asset::MeshMetadata metadata = {
		.Path = Path,
		.KeepGeometryInRAM = KeepGeometryInRAM };
	m_AsyncMeshLoader.RequestAsyncLoad(1, &metadata);
}

DWORD WINAPI AssetManager::ResourceUploadThreadProc(
	_In_ PVOID pParameter)
{
	auto& RenderDevice = RenderDevice::Instance();
	auto& AssetManager = AssetManager::Instance();

	ID3D12Device5* pDevice = RenderDevice.GetDevice();

	ResourceUploadBatch uploader(pDevice);

	// TODO: refactor these, this is here because it is used to generate BLAS when the mesh is uploaded
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList6> commandList;
	UINT64 fenceValue = 0;
	ComPtr<ID3D12Fence> fence;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;
	ThrowIfFailed(pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf())));
	ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf())));
	ThrowIfFailed(pDevice->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_COMPUTE, commandAllocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf())));
	commandList->Close();
	ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));

	while (true)
	{
		std::scoped_lock _(AssetManager.m_UploadCriticalSection);
		AssetManager.m_UploadConditionVariable.Wait(AssetManager.m_UploadCriticalSection, INFINITE);

		if (AssetManager.m_ShutdownThread)
		{
			break;
		}

		uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);
		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		std::vector<std::shared_ptr<Resource>> TrackedScratchBuffers;

		// container to store assets after it has been uploaded to be added to the AssetCache
		std::vector<std::shared_ptr<Asset::Image>> uploadedImages;
		std::vector<std::shared_ptr<Asset::Mesh>> uploadedMeshes;

		// Process Image
		{
			std::shared_ptr<Asset::Image> assetImage;
			while (AssetManager.m_ImageUploadQueue.pop(assetImage, 0))
			{
				const auto& Image = assetImage->Image;
				const auto& Metadata = Image.GetMetadata();

				DXGI_FORMAT format = Metadata.format;
				if (assetImage->Metadata.sRGB)
					format = DirectX::MakeSRGB(format);

				D3D12_RESOURCE_DESC resourceDesc = {};
				switch (Metadata.dimension)
				{
				case TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
					resourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(format,
						static_cast<UINT64>(Metadata.width),
						static_cast<UINT16>(Metadata.arraySize));
					break;

				case TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
					resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format,
						static_cast<UINT64>(Metadata.width),
						static_cast<UINT>(Metadata.height),
						static_cast<UINT16>(Metadata.arraySize));
					break;

				case TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
					resourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(format,
						static_cast<UINT64>(Metadata.width),
						static_cast<UINT>(Metadata.height),
						static_cast<UINT16>(Metadata.depth));
					break;
				}

				std::vector<D3D12_SUBRESOURCE_DATA> subresources(Image.GetImageCount());
				const auto pImages = Image.GetImages();
				for (size_t i = 0; i < Image.GetImageCount(); ++i)
				{
					subresources[i].RowPitch = pImages[i].rowPitch;
					subresources[i].SlicePitch = pImages[i].slicePitch;
					subresources[i].pData = pImages[i].pixels;
				}

				D3D12MA::ALLOCATION_DESC allocationDesc = {};
				allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
				auto Resource = RenderDevice.CreateResource(&allocationDesc, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr);

				uploader.Upload(Resource->pResource.Get(), 0, subresources.data(), subresources.size());

				Descriptor srv = RenderDevice.GetResourceViewHeaps().AllocateResourceView();
				RenderDevice.CreateShaderResourceView(Resource->pResource.Get(), srv);

				assetImage->Resource = std::move(Resource);
				assetImage->SRV = std::move(srv);

				uploadedImages.push_back(assetImage);
			}
		}

		// Process Mesh
		{
			std::shared_ptr<Asset::Mesh> assetMesh;
			while (AssetManager.m_MeshUploadQueue.pop(assetMesh, 0))
			{
				UINT64 vertexBufferSizeInBytes = assetMesh->Vertices.size() * sizeof(Vertex);
				UINT64 indexBufferSizeInBytes = assetMesh->Indices.size() * sizeof(unsigned int);

				D3D12MA::ALLOCATION_DESC allocationDesc = {};
				allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

				auto vertexBuffer = RenderDevice.CreateBuffer(&allocationDesc, vertexBufferSizeInBytes);
				auto indexBuffer = RenderDevice.CreateBuffer(&allocationDesc, indexBufferSizeInBytes);

				// Upload vertex data
				D3D12_SUBRESOURCE_DATA subresource = {};
				subresource.pData = assetMesh->Vertices.data();
				subresource.RowPitch = vertexBufferSizeInBytes;
				subresource.SlicePitch = vertexBufferSizeInBytes;

				uploader.Upload(vertexBuffer->pResource.Get(), 0, &subresource, 1);

				// Upload index data
				subresource.pData = assetMesh->Indices.data();
				subresource.RowPitch = indexBufferSizeInBytes;
				subresource.SlicePitch = indexBufferSizeInBytes;

				uploader.Upload(indexBuffer->pResource.Get(), 0, &subresource, 1);

				assetMesh->VertexResource = std::move(vertexBuffer);
				assetMesh->IndexResource = std::move(indexBuffer);

				for (const auto& submesh : assetMesh->Submeshes)
				{
					D3D12_RAYTRACING_GEOMETRY_DESC dxrGeometryDesc = {};
					dxrGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
					dxrGeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
					dxrGeometryDesc.Triangles.Transform3x4 = NULL;
					dxrGeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
					dxrGeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
					dxrGeometryDesc.Triangles.IndexCount = submesh.IndexCount;
					dxrGeometryDesc.Triangles.VertexCount = submesh.VertexCount;
					dxrGeometryDesc.Triangles.IndexBuffer = assetMesh->IndexResource->pResource->GetGPUVirtualAddress() + submesh.StartIndexLocation * sizeof(unsigned int);
					dxrGeometryDesc.Triangles.VertexBuffer.StartAddress = assetMesh->VertexResource->pResource->GetGPUVirtualAddress() + submesh.BaseVertexLocation * sizeof(Vertex);
					dxrGeometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

					assetMesh->BLAS.AddGeometry(dxrGeometryDesc);
				}

				UINT64 scratchSize, resultSize;
				assetMesh->BLAS.ComputeMemoryRequirements(pDevice, &scratchSize, &resultSize);

				// ASB seems to require a committed resource otherwise PIX gives you invalid
				// parameter when trying to view it in the Event list
				allocationDesc = {};
				allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
				allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
				auto scratch = RenderDevice.CreateBuffer(&allocationDesc, scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				auto result = RenderDevice.CreateBuffer(&allocationDesc, resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

				assetMesh->BLAS.Generate(commandList.Get(), scratch->pResource.Get(), result->pResource.Get());

				assetMesh->AccelerationStructure = std::move(result);
				TrackedScratchBuffers.push_back(std::move(scratch));

				uploadedMeshes.push_back(assetMesh);
			}
		}

		auto future = uploader.End(RenderDevice.GetCopyQueue());
		future.wait();

		commandList->Close();
		commandQueue->ExecuteCommandLists(1, CommandListCast(commandList.GetAddressOf()));
		auto fenceToWaitFor = ++fenceValue;
		commandQueue->Signal(fence.Get(), fenceToWaitFor);
		fence->SetEventOnCompletion(fenceToWaitFor, nullptr);

		for (auto& image : uploadedImages)
		{
			entt::id_type hs = entt::hashed_string(image->Metadata.Path.string().data());

			AssetManager.m_ImageCache.Create(hs);
			auto Asset = AssetManager.m_ImageCache.Load(hs);
			*Asset = std::move(*image);
		}

		for (auto& mesh : uploadedMeshes)
		{
			entt::id_type hs = entt::hashed_string(mesh->Metadata.Path.string().data());

			AssetManager.m_MeshCache.Create(hs);
			auto Asset = AssetManager.m_MeshCache.Load(hs);
			*Asset = std::move(*mesh);
		}
	}

	return EXIT_SUCCESS;
}