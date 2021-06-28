/*********************************************************************
 * Fabric.cpp
 * Copyright (C) 2021 Jayou. All Rights Reserved. 
 * 
 * .
 *********************************************************************/

#include "Fabric.h"
#include "Effects.h"
#include <algorithm>
#include <vector>
#include <cassert>
#include "Common/GeometryGenerator.h"

Fabric::Fabric(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
	               int m, int n, float dx, float dt, float kspring, float kdamper, float kwind)
{
	md3dDevice = device;

	mNumRows = m;
	mNumCols = n;

	assert((m*n) % 256 == 0);

	mVertexCount = m*n;
	mTriangleCount = (m - 1)*(n - 1) * 2;

	mTimeStep = dt;
	mSpatialStep = dx;

	mK[0] = kspring;
	mK[1] = kdamper;
	mK[2] = kwind;

	BuildResources(cmdList);
}

UINT Fabric::RowCount()const
{
	return mNumRows;
}

UINT Fabric::ColumnCount()const
{
	return mNumCols;
}

UINT Fabric::VertexCount()const
{
	return mVertexCount;
}

UINT Fabric::TriangleCount()const
{
	return mTriangleCount;
}

float Fabric::Width()const
{
	return mNumCols*mSpatialStep;
}

float Fabric::Depth()const
{
	return mNumRows*mSpatialStep;
}

float Fabric::SpatialStep()const
{
	return mSpatialStep;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Fabric::VertexMap()const
{
	return mCurrVertexSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Fabric::VelocityMap() const
{
	return mCurrVelocitySrv;
}

UINT Fabric::DescriptorCount()const
{
	return 6;
}

void Fabric::BuildResources(ID3D12GraphicsCommandList* cmdList)
{
	// All the textures for the wave simulation will be bound as a shader resource and
	// unordered access view at some point since we ping-pong the buffers.	

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mNumCols;
	texDesc.Height = mNumRows;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mPrevVertex)));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mCurrVertex)));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mCurrVelocity)));

	//
	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	//

	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mCurrVertex.Get(), 0, num2DSubresources);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mPrevVertexUploadBuffer.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mCurrVertexUploadBuffer.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mCurrVelocityUploadBuffer.GetAddressOf())));

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(this->Width(), this->Depth(), this->RowCount(), this->ColumnCount());

	std::vector<DirectX::XMFLOAT4> vertex(grid.Vertices.size());
	std::vector<DirectX::XMFLOAT4> velocity(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		vertex[i]   = DirectX::XMFLOAT4(grid.Vertices[i].Position.x, grid.Vertices[i].Position.y, grid.Vertices[i].Position.z, 0.0f);
		velocity[i] = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = vertex.data();
	subResourceData.RowPitch = mNumCols*sizeof(DirectX::XMFLOAT4);
	subResourceData.SlicePitch = subResourceData.RowPitch * mNumRows;

	//
	// Schedule to copy the data to the default resource, and change states.
	// Note that mCurrSol is put in the GENERIC_READ state so it can be 
	// read by a shader.
	//

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mPrevVertex.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mPrevVertex.Get(), mPrevVertexUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mPrevVertex.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVertex.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mCurrVertex.Get(), mCurrVertexUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVertex.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	subResourceData.pData = velocity.data();

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVelocity.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mCurrVelocity.Get(), mCurrVelocityUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVelocity.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void Fabric::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
	UINT descriptorSize)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mPrevVertex.Get(), &srvDesc, hCpuDescriptor);
	md3dDevice->CreateShaderResourceView(mCurrVertex.Get(), &srvDesc, hCpuDescriptor.Offset(1, descriptorSize));
	md3dDevice->CreateShaderResourceView(mCurrVelocity.Get(), &srvDesc, hCpuDescriptor.Offset(1, descriptorSize));

	md3dDevice->CreateUnorderedAccessView(mPrevVertex.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));
	md3dDevice->CreateUnorderedAccessView(mCurrVertex.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));
	md3dDevice->CreateUnorderedAccessView(mCurrVelocity.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));

	// Save references to the GPU descriptors. 
	mPrevVertexSrv = hGpuDescriptor;
	mCurrVertexSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mCurrVelocitySrv = hGpuDescriptor.Offset(1, descriptorSize);

	mPrevVertexUav = hGpuDescriptor.Offset(1, descriptorSize);
	mCurrVertexUav = hGpuDescriptor.Offset(1, descriptorSize);
	mCurrVelocityUav = hGpuDescriptor.Offset(1, descriptorSize);
}

void Fabric::Update(
	const GameTimer& gt,
	ID3D12GraphicsCommandList* cmdList,
	ID3D12RootSignature* rootSig,
	ID3D12PipelineState* pso,
	const DirectX::XMFLOAT3& wind)
{
	static float t = 0.0f;

	// Accumulate time.
	t += gt.DeltaTime();

	cmdList->SetPipelineState(pso);
	cmdList->SetComputeRootSignature(rootSig);

	// Only update the simulation at the specified time step.
	if(t >= mTimeStep)
	{
		if (mCurrVelocityState == D3D12_RESOURCE_STATE_GENERIC_READ)
		{
			// The current solution needs to be able to be read by the vertex shader, so change its state to GENERIC_READ.
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVelocity.Get(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			mCurrVelocityState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		// Set the update constants.
		cmdList->SetComputeRoot32BitConstants(0, 3, mK, 0);
		cmdList->SetComputeRoot32BitConstants(0, 1, &mSpatialStep, 3);
		UINT gridSize[2] = { mNumCols, mNumRows };
		cmdList->SetComputeRoot32BitConstants(0, 2, gridSize, 6);
		cmdList->SetComputeRoot32BitConstants(0, 3, &wind, 8);

		cmdList->SetComputeRootDescriptorTable(1, mPrevVertexUav);
		cmdList->SetComputeRootDescriptorTable(2, mCurrVertexUav);
		cmdList->SetComputeRootDescriptorTable(3, mCurrVelocityUav);

		// How many groups do we need to dispatch to cover the wave grid.  
		// Note that mNumRows and mNumCols should be divisible by 16
		// so there is no remainder.
		UINT numGroupsX = mNumCols / 16;
		UINT numGroupsY = mNumRows / 16;
		cmdList->Dispatch(numGroupsX, numGroupsY, 1);
 
		//
		// Ping-pong buffers in preparation for the next update.
		// The previous solution is no longer needed and becomes the target of the next solution in the next update.
		// The current solution becomes the previous solution.
		// The next solution becomes the current solution.
		//

		auto resTemp = mPrevVertex;
		mPrevVertex = mCurrVertex;
		mCurrVertex = resTemp;

		auto srvTemp = mPrevVertexSrv;
		mPrevVertexSrv = mCurrVertexSrv;
		mCurrVertexSrv = srvTemp;

		auto uavTemp = mPrevVertexUav;
		mPrevVertexUav = mCurrVertexUav;
		mCurrVertexUav = uavTemp;

		auto stateTemp = mPrevVertexState;
		mPrevVertexState = mCurrVertexState;
		mCurrVertexState = stateTemp;

		t = 0.0f; // reset time

		if (mCurrVertexState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			// The current solution needs to be able to be read by the vertex shader, so change its state to GENERIC_READ.
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVertex.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
			mCurrVertexState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		
		if (mCurrVelocityState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			// The current solution needs to be able to be read by the vertex shader, so change its state to GENERIC_READ.
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVelocity.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
			mCurrVelocityState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
	}
}

void Fabric::Disturb(
	ID3D12GraphicsCommandList* cmdList,
	ID3D12RootSignature* rootSig,
	ID3D12PipelineState* pso,
	UINT i, UINT j,
	float magnitude)
{
	cmdList->SetPipelineState(pso);
	cmdList->SetComputeRootSignature(rootSig);

	// Set the disturb constants.
	UINT disturbIndex[2] = { j, i };
	cmdList->SetComputeRoot32BitConstants(0, 1, &magnitude, 3);
	cmdList->SetComputeRoot32BitConstants(0, 2, disturbIndex, 4);

	cmdList->SetComputeRootDescriptorTable(2, mCurrVertexUav);
	cmdList->SetComputeRootDescriptorTable(3, mCurrVelocityUav);

	// The current solution is in the GENERIC_READ state so it can be read by the vertex shader.
	// Change it to UNORDERED_ACCESS for the compute shader.  Note that a UAV can still be
	// read in a compute shader.
	if (mCurrVertexState == D3D12_RESOURCE_STATE_GENERIC_READ)
	{
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVertex.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mCurrVertexState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	if (mCurrVelocityState == D3D12_RESOURCE_STATE_GENERIC_READ)
	{
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrVelocity.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mCurrVelocityState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// One thread group kicks off one thread, which displaces the height of one
	// vertex and its neighbors.
	cmdList->Dispatch(1, 1, 1);
}



 