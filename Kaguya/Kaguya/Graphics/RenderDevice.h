#pragma once
#include <Core/D3D12/ShaderCompiler.h>

#include <Core/D3D12/Adapter.h>
#include <Core/D3D12/Device.h>
#include <Core/D3D12/CommandQueue.h>
#include <Core/D3D12/PipelineState.h>
#include <Core/D3D12/RaytracingPipelineState.h>
#include <Core/D3D12/Profiler.h>
#include <Core/D3D12/Resource.h>
#include <Core/D3D12/ResourceUploader.h>
#include <Core/D3D12/RootSignature.h>
#include <Core/D3D12/ShaderCompiler.h>
#include <Core/D3D12/SwapChain.h>
#include <Core/D3D12/Raytracing.h>
#include <Core/D3D12/RaytracingShaderTable.h>

struct RootParameters
{
	struct DescriptorTable
	{
		enum
		{
			ShaderResourceDescriptorTable,
			UnorderedAccessDescriptorTable,
			SamplerDescriptorTable,
			NumRootParameters
		};
	};
};

class RenderDevice
{
public:
	static constexpr DXGI_FORMAT DepthFormat		= DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	static void			 Initialize();
	static void			 Shutdown();
	static RenderDevice& Instance();

	static bool IsValid();

	void Present(bool VSync);

	void Resize(UINT Width, UINT Height);

	void BindGraphicsDescriptorTable(const RootSignature& RootSignature, CommandContext& Context)
	{
		// Assumes the RootSignature was created with AddDescriptorTableRootParameterToBuilder function called.
		const UINT Offset = RootSignature.GetDesc().NumParameters - RootParameters::DescriptorTable::NumRootParameters;
		D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptor = Adapter.GetDevice()->GetResourceDescriptorHeap().hGPU(0);
		D3D12_GPU_DESCRIPTOR_HANDLE SamplerDescriptor  = Adapter.GetDevice()->GetSamplerDescriptorHeap().hGPU(0);

		Context->SetGraphicsRootDescriptorTable(
			RootParameters::DescriptorTable::ShaderResourceDescriptorTable + Offset,
			ResourceDescriptor);
		Context->SetGraphicsRootDescriptorTable(
			RootParameters::DescriptorTable::UnorderedAccessDescriptorTable + Offset,
			ResourceDescriptor);
		Context->SetGraphicsRootDescriptorTable(
			RootParameters::DescriptorTable::SamplerDescriptorTable + Offset,
			SamplerDescriptor);
	}

	void BindComputeDescriptorTable(const RootSignature& RootSignature, CommandContext& Context)
	{
		// Assumes the RootSignature was created with AddDescriptorTableRootParameterToBuilder function called.
		const UINT Offset = RootSignature.GetDesc().NumParameters - RootParameters::DescriptorTable::NumRootParameters;
		D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptor = Adapter.GetDevice()->GetResourceDescriptorHeap().hGPU(0);
		D3D12_GPU_DESCRIPTOR_HANDLE SamplerDescriptor  = Adapter.GetDevice()->GetSamplerDescriptorHeap().hGPU(0);

		Context->SetComputeRootDescriptorTable(
			RootParameters::DescriptorTable::ShaderResourceDescriptorTable + Offset,
			ResourceDescriptor);
		Context->SetComputeRootDescriptorTable(
			RootParameters::DescriptorTable::UnorderedAccessDescriptorTable + Offset,
			ResourceDescriptor);
		Context->SetComputeRootDescriptorTable(
			RootParameters::DescriptorTable::SamplerDescriptorTable + Offset,
			SamplerDescriptor);
	}

	[[nodiscard]] RootSignature CreateRootSignature(
		std::function<void(RootSignatureBuilder&)> Configurator,
		bool									   AddDescriptorTableRootParameters = true);

	[[nodiscard]] PipelineState CreateGraphicsPipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc)
	{
		return PipelineState(Adapter.GetD3D12Device(), &Desc);
	}

	[[nodiscard]] PipelineState CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc)
	{
		return PipelineState(Adapter.GetD3D12Device(), &Desc);
	}

	template<typename PipelineStateStream>
	[[nodiscard]] PipelineState CreatePipelineState(PipelineStateStream& Stream)
	{
		return PipelineState(Adapter.GetD3D12Device5(), Stream);
	}

	[[nodiscard]] RaytracingPipelineState CreateRaytracingPipelineState(
		std::function<void(RaytracingPipelineStateBuilder&)> Configurator);

	auto GetCurrentBackBufferResource() const { return m_SwapChain.GetCurrentBackBufferResource(); }

	Adapter* GetAdapter() noexcept { return &Adapter; }
	Device*	 GetDevice() noexcept { return Adapter.GetDevice(); }

private:
	RenderDevice();
	~RenderDevice();

	RenderDevice(const RenderDevice&) = delete;
	RenderDevice& operator=(const RenderDevice&) = delete;

	void AddDescriptorTableRootParameterToBuilder(RootSignatureBuilder& RootSignatureBuilder);

private:
	Adapter Adapter;

	SwapChain m_SwapChain;

	D3D12_CPU_DESCRIPTOR_HANDLE m_ImGuiFontCpuDescriptor;
	D3D12_GPU_DESCRIPTOR_HANDLE m_ImGuiFontGpuDescriptor;
};
