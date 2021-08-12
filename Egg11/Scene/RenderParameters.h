#pragma once
#include "Cam/Base.h"
#include "Mesh/Mien.h"

#pragma warning( disable : 4512 ) // 'assignment operator could not be generated' because of const member

/// Structure passed as parameter to render calls.
namespace Egg11 { namespace Scene
{
	class RenderParameters
	{
	public:
		/// D3D device reference.
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<ID3D11Buffer> perObjectConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> perFrameConstantBuffer;

		/// Camera.
		Cam::Base::P camera;
		/// Identifier to pick appropriate FlipMesh Mien.
		Mesh::Mien mien;

	};
}}

#pragma warning( default : 4512 ) 
