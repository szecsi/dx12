#pragma once

#include "Mesh/Geometry.h"
#include "Texture2D.h"
#include "TextureCube.h"

namespace Egg {
	namespace Importer {

		Egg::Mesh::Geometry::P ImportSimpleObj(ID3D12Device * device, const std::string & filePath);
		// targetHeight > 0 uniformly rescales the imported mesh (about its
		// own origin, no recentering) so its Y bounding-box extent equals
		// targetHeight -- for matching an imported asset's scale to
		// whatever arbitrary units the rest of a scene uses. 0 (default)
		// keeps the file's native scale.
		Egg::Mesh::Geometry::P ImportWithTangentSpace(ID3D12Device * device, const std::string & filePath, float targetHeight = 0.0f);
		Egg::Mesh::Geometry::P ImportWithTangentSpaceAndRigging(ID3D12Device * device, const std::string & filePath);

		Texture2D ImportTexture2D(ID3D12Device * device, const std::string & filePath);
		TextureCube ImportTextureCube(ID3D12Device* device, const std::string& filePath);
	};
}

