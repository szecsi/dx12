#include "stdafx.h"
#include "Mesh/Importer.h"
#include "Mesh/VertexStream.h"
#include "Mesh/Indexed.h"
#include "Mesh/Instanced.h"
#include "ThrowOnFail.h"
#include <assimp/importer.hpp>      // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postProcess.h> // Post processing flags
#include "App/UtfConverter.h"

using namespace Egg11;

Mesh::Indexed::P Mesh::Importer::fromAiMesh(Microsoft::WRL::ComPtr<ID3D11Device> device, aiMesh* assMesh)
{
	D3D11_INPUT_ELEMENT_DESC elements[64];
	unsigned int cElements = 0;
	unsigned int cOffset = 0;
	unsigned int positionOffset = 0;
	unsigned int normalOffset = 0;
	unsigned int tangentOffset = 0;
	unsigned int binormalOffset = 0;
	unsigned int texcoord0Offset = 0;
	unsigned int blendIndicesOffset = 0;
	unsigned int blendWeightsOffset = 0;

	if(assMesh->HasPositions())
	{
		elements[cElements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[cElements].AlignedByteOffset = positionOffset = cOffset;
		cOffset += sizeof(float) * 3;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
//		const char* semanticNameLiteral = "POSITION";
//		char* semanticName = new char[strlen(semanticNameLiteral)+1];
//		strcpy(semanticName, semanticNameLiteral);
		elements[cElements].SemanticName = "POSITION";//semanticName;
		cElements++;
	}
	if(assMesh->HasNormals())
	{
		elements[cElements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[cElements].AlignedByteOffset = normalOffset = cOffset;
		cOffset += sizeof(float) * 3;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
//		const char* semanticNameLiteral = "NORMAL";
//		char* semanticName = new char[strlen(semanticNameLiteral)+1];
//		strcpy(semanticName, semanticNameLiteral);
//		elements[cElements].SemanticName = semanticName;
		elements[cElements].SemanticName = "NORMAL";
		cElements++;
	}
	if(assMesh->HasTangentsAndBitangents())
	{
		elements[cElements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[cElements].AlignedByteOffset = tangentOffset = cOffset;
		cOffset += sizeof(float) * 3;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
//		const char* semanticNameLiteral = "TANGENT";
//		char* semanticName = new char[strlen(semanticNameLiteral)+1];
//		strcpy(semanticName, semanticNameLiteral);
//		elements[cElements].SemanticName = semanticName;
		elements[cElements].SemanticName = "TANGENT";
		cElements++;
	}
	if(assMesh->HasTangentsAndBitangents())
	{
		elements[cElements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[cElements].AlignedByteOffset = binormalOffset = cOffset;
		cOffset += sizeof(float) * 3;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
//		const char* semanticNameLiteral = "BINORMAL";
//		char* semanticName = new char[strlen(semanticNameLiteral)+1];
//		strcpy(semanticName, semanticNameLiteral);
//		elements[cElements].SemanticName = semanticName;
		elements[cElements].SemanticName = "BINORMAL";
		cElements++;
	}
	if(assMesh->HasTextureCoords(0))
	{
		elements[cElements].Format = DXGI_FORMAT_R32G32_FLOAT;
		elements[cElements].AlignedByteOffset = texcoord0Offset = cOffset;
		cOffset += sizeof(float) * 2;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
//		const char* semanticNameLiteral = "TEXCOORD";
//		char* semanticName = new char[strlen(semanticNameLiteral)+1];
//		strcpy(semanticName, semanticNameLiteral);
//		elements[cElements].SemanticName = semanticName;
		elements[cElements].SemanticName = "TEXCOORD";
		cElements++;
	}
	if(assMesh->HasBones())
	{
		elements[cElements].Format = DXGI_FORMAT_R32G32B32A32_UINT;
		elements[cElements].AlignedByteOffset = blendIndicesOffset = cOffset;
		cOffset += sizeof(unsigned int) * 4;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
		elements[cElements].SemanticName = "BLENDINDICES";
		cElements++;

		elements[cElements].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		elements[cElements].AlignedByteOffset = blendWeightsOffset = cOffset;
		cOffset += sizeof(float) * 4;
		elements[cElements].InputSlot = 0;
		elements[cElements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elements[cElements].InstanceDataStepRate = 0;
		elements[cElements].SemanticIndex = 0;
		elements[cElements].SemanticName = "BLENDWEIGHTS";
		cElements++;
	}

	unsigned int vertexStride = cOffset;
	unsigned int nElements = cElements;
	unsigned int nVertices = assMesh->mNumVertices;

	char* sysMemVertices = new char[nVertices * vertexStride];

	for(unsigned int iVertex=0; iVertex < assMesh->mNumVertices; iVertex++)
	{
		memcpy(sysMemVertices + iVertex * vertexStride + positionOffset, &assMesh->mVertices[iVertex], sizeof(float) * 3);
		if(assMesh->HasNormals())
			memcpy(sysMemVertices + iVertex * vertexStride + normalOffset, &assMesh->mNormals[iVertex], sizeof(float) * 3);
		if(assMesh->HasTangentsAndBitangents())
		{
			memcpy(sysMemVertices + iVertex * vertexStride + tangentOffset, &assMesh->mTangents[iVertex], sizeof(float) * 3);
			memcpy(sysMemVertices + iVertex * vertexStride + binormalOffset, &assMesh->mBitangents[iVertex], sizeof(float) * 3);
		}
		if(assMesh->HasTextureCoords(0))
			memcpy(sysMemVertices + iVertex * vertexStride + texcoord0Offset, &assMesh->mTextureCoords[0][iVertex], sizeof(float) * 2);
	}
	if(assMesh->HasBones())
	{
		for(unsigned int iVertex=0; iVertex < assMesh->mNumVertices; iVertex++)
		{
			float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
			memcpy(sysMemVertices + iVertex * vertexStride + blendWeightsOffset, &zeros, sizeof(float) * 4);
			int zeroInts[4] = {0, 0, 0, 0};
			memcpy(sysMemVertices + iVertex * vertexStride + blendIndicesOffset, &zeroInts, sizeof(int) * 4);
		}

		char* nWeightsPerVertex = new char[assMesh->mNumVertices];
		for(int i=0; i<assMesh->mNumVertices; i++)
			nWeightsPerVertex[i] = 0;
		for(unsigned int iBone=0; iBone<assMesh->mNumBones; iBone++)
		{
			for(unsigned int iWeights=0; iWeights<assMesh->mBones[iBone]->mNumWeights; iWeights++)
			{
				unsigned int iVertex = assMesh->mBones[iBone]->mWeights[iWeights].mVertexId;
				float weight = assMesh->mBones[iBone]->mWeights[iWeights].mWeight;
				memcpy(sysMemVertices + iVertex * vertexStride + blendWeightsOffset + nWeightsPerVertex[iVertex]*sizeof(float), &weight, sizeof(float));
				memcpy(sysMemVertices + iVertex * vertexStride + blendIndicesOffset + nWeightsPerVertex[iVertex]*sizeof(int), &iBone, sizeof(int));
				nWeightsPerVertex[iVertex]++;
			}			
		}
		delete nWeightsPerVertex;
	}

	Mesh::VertexStreamDesc vertexStreamDesc;
	vertexStreamDesc.elements = elements;
	vertexStreamDesc.nElements = nElements;
	vertexStreamDesc.nVertices = nVertices;
	vertexStreamDesc.vertexData = sysMemVertices;
	vertexStreamDesc.vertexStride = vertexStride;

	Mesh::VertexStream::P vertexStream = Mesh::VertexStream::create(device, vertexStreamDesc);

	unsigned int nPrimitives = assMesh->mNumFaces;
	bool wideIndexBuffer = nVertices > USHRT_MAX;

	Mesh::IndexBufferDesc indexBufferDesc;
	indexBufferDesc.nIndices = nPrimitives * 3;
	indexBufferDesc.nPrimitives = nPrimitives;
	indexBufferDesc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	if(wideIndexBuffer)
	{
		unsigned int* sysMemIndices = new unsigned int[nPrimitives*3];
		for(int iFace=0; iFace < nPrimitives; iFace++)
		{
			sysMemIndices[iFace * 3 + 0] = assMesh->mFaces[iFace].mIndices[0];
			sysMemIndices[iFace * 3 + 1] = assMesh->mFaces[iFace].mIndices[1];
			sysMemIndices[iFace * 3 + 2] = assMesh->mFaces[iFace].mIndices[2];
		}
		indexBufferDesc.indexData = sysMemIndices;
		indexBufferDesc.indexFormat = DXGI_FORMAT_R32_UINT;
	}
	else
	{
		unsigned short* sysMemIndices = new unsigned short[nPrimitives*3];
		for(int iFace=0; iFace < nPrimitives; iFace++)
		{
			sysMemIndices[iFace * 3 + 0] = assMesh->mFaces[iFace].mIndices[0];
			sysMemIndices[iFace * 3 + 1] = assMesh->mFaces[iFace].mIndices[1];
			sysMemIndices[iFace * 3 + 2] = assMesh->mFaces[iFace].mIndices[2];
		}
		indexBufferDesc.indexData = sysMemIndices;
		indexBufferDesc.indexFormat = DXGI_FORMAT_R16_UINT;
	}

	Mesh::Indexed::P indexed = Mesh::Indexed::createFromSingleStream(device, indexBufferDesc, vertexStream);

	delete vertexStreamDesc.vertexData;
	delete indexBufferDesc.indexData;

	return indexed;
}
