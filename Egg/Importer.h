#pragma once

#include "Mesh/Geometry.h"
#include "Texture2D.h"
#include "TextureCube.h"

namespace Egg {
	namespace Importer {

		Egg::Mesh::Geometry::P ImportSimpleObj(ID3D12Device * device, const std::string & filePath);
		Egg::Mesh::Geometry::P ImportWithTangentSpace(ID3D12Device * device, const std::string & filePath);

		Texture2D ImportTexture2D(ID3D12Device * device, const std::string & filePath);
		TextureCube ImportTextureCube(ID3D12Device* device, const std::string& filePath);
	};
}

