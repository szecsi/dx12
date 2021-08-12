#pragma once

#include "Mesh/Indexed.h"
#include "Mesh/Multi.h"
#include "Mesh/InputBinder.h"
#include "Mesh/Mien.h"

struct aiMesh;

namespace Egg11 { namespace Mesh
{
	class Geometry;

	class Importer
	{
	public:
		static Mesh::Indexed::P fromAiMesh(Microsoft::WRL::ComPtr<ID3D11Device> device, aiMesh* assMesh);
	};

}} // namespace Egg11::Mesh