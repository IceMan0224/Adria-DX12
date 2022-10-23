#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "../Core/Definitions.h"

namespace adria
{
	struct GlobalBlackboardData
	{
		DirectX::XMVECTOR			camera_position;
		DirectX::XMMATRIX			camera_view;
		DirectX::XMMATRIX			camera_proj;
		DirectX::XMMATRIX			camera_viewproj;
		float32						camera_fov;
		D3D12_GPU_VIRTUAL_ADDRESS   new_frame_cbuffer_address;
		D3D12_GPU_VIRTUAL_ADDRESS   frame_cbuffer_address;
		D3D12_GPU_VIRTUAL_ADDRESS   weather_cbuffer_address;
		D3D12_GPU_VIRTUAL_ADDRESS   compute_cbuffer_address;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE null_uav_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texturecube;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texture2darray;
		D3D12_CPU_DESCRIPTOR_HANDLE white_srv_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE lights_buffer_cpu_srv;
		D3D12_GPU_DESCRIPTOR_HANDLE lights_buffer_gpu_srv;
	};

	struct DoFData
	{
		float32 dof_params_x;
		float32 dof_params_y;
		float32 dof_params_z;
		float32 dof_params_w;
	};
}