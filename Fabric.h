/*********************************************************************
 * Fabric.h
 * Copyright (C) 2021 Jayou. All Rights Reserved. 
 * 
 * .
 *********************************************************************/

#ifndef FABRIC_H
#define FABRIC_H

#include "Common/d3dUtil.h"
#include "Common/GameTimer.h"

class Fabric
{
public:
	// Note that m,n should be divisible by 16 so there is no 
	// remainder when we divide into thread groups.
	Fabric(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, int m, int n, float dx, float dt, float kspring, float kdamper, float kwind);
	Fabric(const Fabric& rhs) = delete;
	Fabric& operator=(const Fabric& rhs) = delete;
	~Fabric()=default;

	UINT RowCount()const;
	UINT ColumnCount()const;
	UINT VertexCount()const;
	UINT TriangleCount()const;
	float Width()const;
	float Depth()const;
	float SpatialStep()const;

	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexMap()const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE VelocityMap()const;

	UINT DescriptorCount()const;

	void BuildResources(ID3D12GraphicsCommandList* cmdList);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
		UINT descriptorSize);

	void Update(
		const GameTimer& gt,
		ID3D12GraphicsCommandList* cmdList, 
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* pso,
		const DirectX::XMFLOAT3& wind);

	void Disturb(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* pso, 
		UINT i, UINT j, 
		float magnitude);

private:

	UINT mNumRows;
	UINT mNumCols;

	UINT mVertexCount;
	UINT mTriangleCount;

	// Simulation constants we can precompute.
	float mK[3];

	float mTimeStep;
	float mSpatialStep;

	ID3D12Device* md3dDevice = nullptr;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mPrevVertexSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrVertexSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrVelocitySrv;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mPrevVertexUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrVertexUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrVelocityUav;	

	// Two for ping-ponging the textures.
	Microsoft::WRL::ComPtr<ID3D12Resource> mPrevVertex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mCurrVertex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mCurrVelocity = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mPrevVertexUploadBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mCurrVertexUploadBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mCurrVelocityUploadBuffer = nullptr;

	D3D12_RESOURCE_STATES mPrevVertexState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES mCurrVertexState = D3D12_RESOURCE_STATE_GENERIC_READ;
	D3D12_RESOURCE_STATES mCurrVelocityState = D3D12_RESOURCE_STATE_GENERIC_READ;
};

#endif // CLOTH_H