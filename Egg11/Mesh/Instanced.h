#pragma once
#include "geometry.h"
#include "Mesh/Indexed.h"


namespace Egg11 { namespace Mesh
{
	struct InstanceBufferDesc
	{
		static const D3D11_INPUT_ELEMENT_DESC defaultElements[3];
		static const float defaultData[12];
	
		InstanceBufferDesc():elements(defaultElements),nElements(3),instanceStride(12*sizeof(float)), instanceData((void*)defaultData) {}

		const D3D11_INPUT_ELEMENT_DESC* elements;
		unsigned int nElements;
		unsigned int instanceStride;
		void* instanceData;
	};

	class Indexed;

	/// Geometry with instancing information.
	GG_SUBCLASS(Instanced, Mesh::Geometry)
		Microsoft::WRL::ComPtr<ID3D11Buffer>* vertexBuffers;
		unsigned int* vertexStrides;
		unsigned int nVertexBuffers;
		unsigned int nIndexedVertexBuffers;

		const D3D11_INPUT_ELEMENT_DESC* elements;
		unsigned int nElements;

		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	
		DXGI_FORMAT indexFormat;
		unsigned int nIndices;
		unsigned int nPrimitives;

		D3D11_PRIMITIVE_TOPOLOGY topology;

		unsigned int nInstances;
		Instanced(Microsoft::WRL::ComPtr<ID3D11Device> device, unsigned int nInstances, InstanceBufferDesc* desc, unsigned int nInstanceBuffers, Mesh::Indexed::P indexed);
	public:
	
		~Instanced(void);

		/// Gets vertex element description.
		void getElements(const D3D11_INPUT_ELEMENT_DESC*& elements, unsigned int& nElements);
		/// Renders geometry with instancing.
		void draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

		Microsoft::WRL::ComPtr<ID3D11Buffer> getPrimaryBuffer(){return vertexBuffers[0];}
		Microsoft::WRL::ComPtr<ID3D11Buffer> getVertexBuffer(unsigned int iVertexBuffer) { return vertexBuffers[iVertexBuffer];}
		Microsoft::WRL::ComPtr<ID3D11Buffer> getInstanceBuffer(unsigned int iInstanceBuffer) { return vertexBuffers[iInstanceBuffer + nIndexedVertexBuffers];}
	GG_ENDCLASS

}} // namespace Egg11::Mesh