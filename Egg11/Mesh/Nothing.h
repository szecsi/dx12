#pragma once
#include "Mesh/Geometry.h"
#include <vector>

namespace Egg11 { namespace Mesh
{

	GG_SUBCLASS(Nothing, Mesh::Geometry)
		unsigned int nVertices;
		D3D11_PRIMITIVE_TOPOLOGY topology;
	public:
		Nothing(unsigned int nVertices, D3D11_PRIMITIVE_TOPOLOGY topology);
	
		using A = std::vector< Nothing::P >;
		~Nothing(void);

		void getElements(const D3D11_INPUT_ELEMENT_DESC*& elements, unsigned int& nElements);
		unsigned int getElementCount() {return 0;}

		void draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
		Microsoft::WRL::ComPtr<ID3D11Buffer> getPrimaryBuffer(){return NULL;}

		unsigned int getStride() {return 0;}

	GG_ENDCLASS


}} // namespace Egg11::Mesh