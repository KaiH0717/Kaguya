#pragma once
#include <Core/CoreDefines.h>

#include "DeviceChild.h"
#include "CommandQueue.h"
#include "LinearAllocator.h"

class CommandContext : public DeviceChild
{
public:
	CommandContext(Device* Device, ECommandQueueType Type, D3D12_COMMAND_LIST_TYPE CommandListType);

	CommandQueue* GetCommandQueue();

	CommandListHandle& operator->() { return CommandListHandle; }

	void OpenCommandList()
	{
		if (!CommandAllocator)
		{
			CommandAllocator = CommandAllocatorPool.RequestCommandAllocator();
		}

		CommandListHandle = GetCommandQueue()->RequestCommandList(CommandAllocator);

		CpuConstantAllocator.Begin(GetCommandQueue()->GetCompletedValue());
	}

	void CloseCommandList() { CommandListHandle.Close(); }

	// Returns CommandSyncPoint, may be ignored if WaitForCompletion is true
	CommandSyncPoint Execute(bool WaitForCompletion);

	void TransitionBarrier(
		Resource*			  Resource,
		D3D12_RESOURCE_STATES State,
		UINT				  Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void AliasingBarrier(Resource* BeforeResource, Resource* AfterResource);

	void UAVBarrier(Resource* Resource);

	void FlushResourceBarriers();

	void BindResourceViewHeaps();

	// These version of the API calls should be used as it needs to flush resource barriers before any work
	void DrawInstanced(
		_In_ UINT VertexCount,
		_In_ UINT InstanceCount,
		_In_ UINT StartVertexLocation,
		_In_ UINT StartInstanceLocation);

	void DrawIndexedInstanced(
		_In_ UINT IndexCount,
		_In_ UINT InstanceCount,
		_In_ UINT StartIndexLocation,
		_In_ UINT BaseVertexLocation,
		_In_ UINT StartInstanceLocation);

	void Dispatch(_In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY, _In_ UINT ThreadGroupCountZ);

	template<UINT ThreadSizeX>
	void Dispatch1D(_In_ UINT ThreadGroupCountX)
	{
		ThreadGroupCountX = RoundUpAndDivide(ThreadGroupCountX, ThreadSizeX);

		Dispatch(ThreadGroupCountX, 1, 1);
	}

	template<UINT ThreadSizeX, UINT ThreadSizeY>
	void Dispatch2D(_In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY)
	{
		ThreadGroupCountX = RoundUpAndDivide(ThreadGroupCountX, ThreadSizeX);
		ThreadGroupCountY = RoundUpAndDivide(ThreadGroupCountY, ThreadSizeY);

		Dispatch(ThreadGroupCountX, ThreadGroupCountY, 1);
	}

	template<UINT ThreadSizeX, UINT ThreadSizeY, UINT ThreadSizeZ>
	void Dispatch3D(_In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY, _In_ UINT ThreadGroupCountZ)
	{
		ThreadGroupCountX = RoundUpAndDivide(ThreadGroupCountX, ThreadSizeX);
		ThreadGroupCountY = RoundUpAndDivide(ThreadGroupCountY, ThreadSizeY);
		ThreadGroupCountZ = RoundUpAndDivide(ThreadGroupCountZ, ThreadSizeZ);

		Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	void DispatchRays(_In_ const D3D12_DISPATCH_RAYS_DESC* pDesc);

	void DispatchMesh(_In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY, _In_ UINT ThreadGroupCountZ);

	void ResetCounter(Resource* CounterResource, UINT64 CounterOffset, UINT Value = 0)
	{
		Allocation Allocation = CpuConstantAllocator.Allocate(sizeof(UINT));
		std::memcpy(Allocation.CPUVirtualAddress, &Value, sizeof(UINT));

		CommandListHandle->CopyBufferRegion(
			CounterResource->GetResource(),
			CounterOffset,
			Allocation.pResource,
			Allocation.Offset,
			sizeof(UINT));
	}

	const ECommandQueueType Type;

	CommandListHandle	 CommandListHandle;
	CommandAllocator*	 CommandAllocator;
	CommandAllocatorPool CommandAllocatorPool;

	LinearAllocator CpuConstantAllocator;
};