#include "stdafx.h"
#include "Mesh/Indexed.h"
#include "D3DEnumOps.h"
#include "ThrowOnFail.h"

using namespace Egg11;

const unsigned short Mesh::IndexBufferDesc::defaultData[6] = { 0, 1, 2, 3, 2, 1};


Mesh::Indexed::Indexed(Microsoft::WRL::ComPtr<ID3D11Device> device, const IndexBufferDesc& desc, VertexStream::A& vertexStreams)
{
	this->nVertexBuffers = vertexStreams.size();
	this->vertexBuffers = new Microsoft::WRL::ComPtr<ID3D11Buffer>[nVertexBuffers];
	this->vertexStrides = new unsigned int[nVertexBuffers];

	for(int iVertexBuffer=0; iVertexBuffer < nVertexBuffers; iVertexBuffer++)
	{
		this->vertexBuffers[iVertexBuffer] = vertexStreams.at(iVertexBuffer)->getBuffer();
		this->vertexStrides[iVertexBuffer] = vertexStreams.at(iVertexBuffer)->getStride();
	}

	nElements = 0;
	for(int iVertexBuffer=0; iVertexBuffer<nVertexBuffers; iVertexBuffer++)
	{
		nElements += vertexStreams.at(iVertexBuffer)->getElementCount();
	}
	D3D11_INPUT_ELEMENT_DESC* elements = new D3D11_INPUT_ELEMENT_DESC[nElements];
	nElements = 0;
	for(int iVertexBuffer=0; iVertexBuffer<nVertexBuffers; iVertexBuffer++)
	{
		const D3D11_INPUT_ELEMENT_DESC* vbElements;
		unsigned int vbnElements;
		vertexStreams.at(iVertexBuffer)->getElements(vbElements, vbnElements);
		for(int iElement=0; iElement<vbnElements; iElement++, nElements++)
		{
			elements[nElements] = vbElements[iElement];
			elements[nElements].InputSlot = iVertexBuffer;
			char* semanticName = new char[strlen(vbElements[iElement].SemanticName)+1];
			strcpy(semanticName, vbElements[iElement].SemanticName);
			elements[nElements].SemanticName = semanticName;
		}
	}
	this->elements = elements;

	this->nPrimitives = desc.nPrimitives;
	this->indexFormat = desc.indexFormat;
	this->topology = desc.topology;

	this->nIndices = D3DEnumOps::getIndexCount(topology, nPrimitives);
	
	if(desc.nIndices != 0)
	{
		if(desc.nIndices != this->nIndices
			&& topology != D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
			&& topology != D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ
			&& topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
			&& topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
			throw HrException(E_INVALIDARG, "Index count is not consistent with topology and primitive count.", __FILE__, __LINE__);
		this->nIndices = desc.nIndices;
	}

	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.BindFlags = desc.bindFlags;
	indexBufferDesc.ByteWidth = nIndices * desc.getIndexStride();
	indexBufferDesc.CPUAccessFlags = desc.cpuAccessFlags;
	indexBufferDesc.MiscFlags = desc.miscFlags;
	indexBufferDesc.StructureByteStride = 0;
	indexBufferDesc.Usage = desc.usage;

	D3D11_SUBRESOURCE_DATA initialIndexData;
	initialIndexData.pSysMem = desc.indexData;

	ThrowOnFail("Could not create index buffer.", __FILE__, __LINE__) = 
		device->CreateBuffer(&indexBufferDesc, &initialIndexData, indexBuffer.GetAddressOf());

}


Mesh::Indexed::~Indexed(void)
{
	delete [] vertexBuffers;
	delete [] vertexStrides;
	for(int i=0; i<nElements; i++)
		if(elements[i].SemanticName)
			delete elements[i].SemanticName;
	delete [] elements;
}

void Mesh::Indexed::getElements(const D3D11_INPUT_ELEMENT_DESC*& elements, unsigned int& nElements)
{
	elements = this->elements;
	nElements = this->nElements;
}

void Mesh::Indexed::draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	context->IASetPrimitiveTopology(topology);
	context->IASetIndexBuffer(indexBuffer.Get(), indexFormat, 0);

	static const unsigned int zeros[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	context->IASetVertexBuffers(0, nVertexBuffers, reinterpret_cast<ID3D11Buffer**>(vertexBuffers), vertexStrides, zeros);

	context->DrawIndexed(nIndices, 0, 0);


}
