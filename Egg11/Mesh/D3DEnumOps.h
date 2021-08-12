#pragma once
#include "ThrowOnFail.h"

namespace Egg11 { namespace Mesh
{
	struct D3DEnumOps
	{
		static unsigned int getFormatByteWidth(DXGI_FORMAT format)
		{
			switch(format)
			{
				case DXGI_FORMAT_R16_UINT : return sizeof(unsigned short);
				case DXGI_FORMAT_R32_SINT :
				case DXGI_FORMAT_R32_UINT :
				case DXGI_FORMAT_R32_FLOAT : return sizeof(float);
				case DXGI_FORMAT_R32G32_SINT:
				case DXGI_FORMAT_R32G32_UINT:
				case DXGI_FORMAT_R32G32_FLOAT : return sizeof(float) * 2;
				case DXGI_FORMAT_R32G32B32_SINT:
				case DXGI_FORMAT_R32G32B32_UINT:
				case DXGI_FORMAT_R32G32B32_FLOAT : return sizeof(float) * 3;
				case DXGI_FORMAT_R32G32B32A32_SINT:
				case DXGI_FORMAT_R32G32B32A32_UINT:
				case DXGI_FORMAT_R32G32B32A32_FLOAT : return sizeof(float) * 4;
				default:
					return 0;
			};
		}

		static unsigned int getIndexCount(D3D11_PRIMITIVE_TOPOLOGY topology, unsigned int nPrimitives)
		{
			unsigned int nIndices = 0;
			switch(topology)
			{
				case D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED: throw HrException(E_INVALIDARG, "Index buffer primitive topology undefined.", __FILE__, __LINE__);
				case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST: nIndices = nPrimitives; break;
				case D3D11_PRIMITIVE_TOPOLOGY_LINELIST: nIndices = nPrimitives*2; break;
				case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP: nIndices = nPrimitives+1; break; // WARNING
				case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST: nIndices = nPrimitives*3; break;
				case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: nIndices = nPrimitives+2; break; // WARNING
				case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: nIndices = nPrimitives*4; break;
				case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ: nIndices = nPrimitives+3; break;	 // WARNING
				case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ: nIndices = nPrimitives*6; break;
				case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ: nIndices = nPrimitives*2+4; break; // WARNING
				case D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives; break;
				case D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*2; break;
				case D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*3; break;
				case D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*4; break;
				case D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*5; break;
				case D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*6; break;
				case D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*7; break;
				case D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*8; break;
				case D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST	: nIndices = nPrimitives*9; break;
				case D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*10; break;
				case D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*11; break;
				case D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*12; break;
				case D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*13; break;
				case D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*14; break;
				case D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*15; break;
				case D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*16; break;
				case D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*17; break;
				case D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*18; break;
				case D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*19; break;
				case D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*20; break;
				case D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*21; break;
				case D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*22; break;
				case D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*23; break;
				case D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*24; break;
				case D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*25; break;
				case D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*26; break;
				case D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*27; break;
				case D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*28; break;
				case D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*29; break;
				case D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*30; break;
				case D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*31; break;
				case D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST: nIndices = nPrimitives*32; break;
			};
			return nIndices;
		}
	};

}} // namespace Egg11::Mesh