#include "pch.h"
#include "CommandContext.h"
#include "Device.h"

CommandContext::CommandContext(Device* Device, ECommandQueueType Type, D3D12_COMMAND_LIST_TYPE CommandListType)
	: DeviceChild(Device)
	, Type(Type)
	, CommandListHandle(Device, CommandListType, Device->GetCommandQueue(Type))
	, CommandAllocator(nullptr)
	, CommandAllocatorPool(Device, CommandListType)
	, CpuConstantAllocator(Device->GetDevice())
{
}

CommandQueue* CommandContext::GetCommandQueue()
{
	return GetParentDevice()->GetCommandQueue(Type);
}

CommandSyncPoint CommandContext::Execute(bool WaitForCompletion)
{
	CommandQueue* CommandQueue = GetCommandQueue();
	CommandQueue->ExecuteCommandLists(1, &CommandListHandle, WaitForCompletion);

	assert(CommandAllocator && "Invalid CommandAllocator");

	CommandSyncPoint SyncPoint = CommandAllocator->GetSyncPoint();
	// Release the command allocator so it can be reused.
	CommandAllocatorPool.DiscardCommandAllocator(CommandAllocator);
	CommandAllocator = nullptr;

	CpuConstantAllocator.End(SyncPoint.GetValue());
	return SyncPoint;
}

void CommandContext::TransitionBarrier(
	Resource*			  Resource,
	D3D12_RESOURCE_STATES State,
	UINT				  Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	CommandListHandle.TransitionBarrier(Resource, State, Subresource);
}

void CommandContext::AliasingBarrier(Resource* BeforeResource, Resource* AfterResource)
{
	CommandListHandle.AliasingBarrier(BeforeResource, AfterResource);
}

void CommandContext::UAVBarrier(Resource* Resource)
{
	CommandListHandle.UAVBarrier(Resource);
}

void CommandContext::FlushResourceBarriers()
{
	CommandListHandle.FlushResourceBarriers();
}

void CommandContext::BindResourceViewHeaps()
{
	ID3D12DescriptorHeap* DescriptorHeaps[2] = {
		GetParentDevice()->GetResourceViewHeaps().GetResourceDescriptorHeap(),
		GetParentDevice()->GetResourceViewHeaps().GetSamplerDescriptorHeap(),
	};

	CommandListHandle->SetDescriptorHeaps(2, DescriptorHeaps);
}

void CommandContext::DrawInstanced(
	_In_ UINT VertexCount,
	_In_ UINT InstanceCount,
	_In_ UINT StartVertexLocation,
	_In_ UINT StartInstanceLocation)
{
	CommandListHandle.FlushResourceBarriers();
	CommandListHandle->DrawInstanced(VertexCount, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CommandContext::DrawIndexedInstanced(
	_In_ UINT IndexCount,
	_In_ UINT InstanceCount,
	_In_ UINT StartIndexLocation,
	_In_ UINT BaseVertexLocation,
	_In_ UINT StartInstanceLocation)
{
	CommandListHandle.FlushResourceBarriers();
	CommandListHandle->DrawIndexedInstanced(
		IndexCount,
		InstanceCount,
		StartIndexLocation,
		BaseVertexLocation,
		StartInstanceLocation);
}

void CommandContext::Dispatch(_In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY, _In_ UINT ThreadGroupCountZ)
{
	CommandListHandle.FlushResourceBarriers();
	CommandListHandle->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void CommandContext::DispatchRays(_In_ const D3D12_DISPATCH_RAYS_DESC* pDesc)
{
	CommandListHandle.FlushResourceBarriers();
	CommandListHandle.GetGraphicsCommandList4()->DispatchRays(pDesc);
}

void CommandContext::DispatchMesh(_In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY, _In_ UINT ThreadGroupCountZ)
{
	CommandListHandle.FlushResourceBarriers();
	CommandListHandle.GetGraphicsCommandList6()->DispatchMesh(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}