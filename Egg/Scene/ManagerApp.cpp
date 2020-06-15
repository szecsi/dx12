#include "ManagerApp.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Egg/Vertex.h"

using namespace Egg::Scene;

Egg::Mesh::IndexedGeometry::P indexedMeshFromAssimpMesh(ID3D12Device* device, aiMesh* mesh) {
	std::vector<unsigned int> indices;
	std::vector<Egg::PNT_Vertex> vertices;
	indices.reserve(mesh->mNumFaces);
	vertices.reserve(mesh->mNumVertices);

	Egg::PNT_Vertex v;

	for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
		v.position.x = mesh->mVertices[i].x;
		v.position.y = mesh->mVertices[i].y;
		v.position.z = mesh->mVertices[i].z;

		v.normal.x = mesh->mNormals[i].x;
		v.normal.y = mesh->mNormals[i].y;
		v.normal.z = mesh->mNormals[i].z;

		v.tex.x = mesh->mTextureCoords[0][i].x;
		v.tex.y = mesh->mTextureCoords[0][i].y;

		vertices.emplace_back(v);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			indices.emplace_back(face.mIndices[j]);
		}
	}

	Egg::Mesh::IndexedGeometry::P geometry = Egg::Mesh::IndexedGeometry::Create(device, &(vertices.at(0)), (unsigned int)(vertices.size() * sizeof(Egg::PNT_Vertex)), (unsigned int)sizeof(Egg::PNT_Vertex),
		&(indices.at(0)), (unsigned int)(indices.size() * 4), DXGI_FORMAT_R32_UINT);

	geometry->AddInputElement({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	geometry->AddInputElement({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	geometry->AddInputElement({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

	return geometry;
}

Egg::Mesh::Multi::P ManagerApp::LoadMultiMesh(
	const std::string& filename,
	unsigned int flags,
	const std::string& alias) {

	std::string path = "../Media/" + filename;

	Assimp::Importer importer;

	if (flags == -1) {
		flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_GenUVCoords;
	}
	const aiScene* scene = importer.ReadFile(path, flags);

	ASSERT(scene != nullptr, "Failed to load obj file: '%s'. Assimp error message: '%s'", path.c_str(), importer.GetErrorString());

	ASSERT(scene->HasMeshes(), "Obj file: '%s' does not contain a mesh.", path.c_str());

	using namespace Egg::Mesh;

	Multi::P multiMesh = Multi::Create();

	for (int i = 0; i < scene->mNumMeshes; i++)
	{
		IndexedGeometry::P indexedGeometry = indexedMeshFromAssimpMesh(device.Get(), scene->mMeshes[i]);
		if (!alias.empty())
			indexedGeometries[alias + "[" + std::to_string(i) + "]"] = indexedGeometry;

		aiString texturePath;
		scene->mMaterials[scene->mMeshes[i]->mMaterialIndex]->GetTexture(aiTextureType::aiTextureType_DIFFUSE, 0, &texturePath);

		Mesh::Flip::P flipMesh = Mesh::Flip::Create();
		AddDefaultShadedMeshes(flipMesh, indexedGeometry, texturePath.data);

		multiMesh->Add(flipMesh);
	}
	return multiMesh;


}
