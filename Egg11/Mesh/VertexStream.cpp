#include "stdafx.h"
#include "Mesh/VertexStream.h"
#include "ThrowOnFail.h"

using namespace Egg11;

const D3D11_INPUT_ELEMENT_DESC Mesh::VertexStreamDesc::defaultElements[2] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0} };
const float Mesh::VertexStreamDesc::defaultData[20] = { 
	-1, 1, 0.9999f, 0, 0,		1, 1, 0.9999f, 1, 0,		-1, -1, 0.9999f, 0, 1,		1, -1, 0.9999f, 1, 1};


Mesh::VertexStream::VertexStream(Microsoft::WRL::ComPtr<ID3D11Device> device, const VertexStreamDesc& desc)
{
	this->nElements = desc.nElements;
	this->elements = new D3D11_INPUT_ELEMENT_DESC[nElements];
	memcpy(this->elements, desc.elements, nElements * sizeof(D3D11_INPUT_ELEMENT_DESC));

	for(int i=0; i<nElements; i++)
	{
		char* semanticName = new char[strlen(desc.elements[i].SemanticName)+1];
		strcpy(semanticName, desc.elements[i].SemanticName);
		this->elements[i].SemanticName = semanticName;
	}

	this->nVertices = desc.nVertices;
	this->vertexStride = desc.vertexStride;

	D3D11_BUFFER_DESC vertexBufferDesc;
	vertexBufferDesc.BindFlags = desc.bindFlags;
	vertexBufferDesc.ByteWidth = nVertices * vertexStride;
	vertexBufferDesc.CPUAccessFlags = desc.cpuAccessFlags;
	vertexBufferDesc.MiscFlags = desc.miscFlags;
	vertexBufferDesc.StructureByteStride = vertexStride;
	vertexBufferDesc.Usage = desc.usage;

	D3D11_SUBRESOURCE_DATA initialVertexData;
	initialVertexData.pSysMem = desc.vertexData;

	ThrowOnFail("Could not create vertex buffer.", __FILE__, __LINE__) = 
		device->CreateBuffer(&vertexBufferDesc, &initialVertexData, vertexBuffer.GetAddressOf());
}

Mesh::VertexStream::~VertexStream(void)
{
	for(int i=0; i<nElements; i++)
		if(elements[i].SemanticName)
			delete elements[i].SemanticName;
	delete [] elements;
}

void Mesh::VertexStream::getElements(const D3D11_INPUT_ELEMENT_DESC*& elements, unsigned int& nElements)
{
	elements = this->elements;
	nElements = this->nElements;
}

void Mesh::VertexStream::draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	unsigned int offset = 0;
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &vertexStride, &offset);
	context->Draw(nVertices, 0);
}
