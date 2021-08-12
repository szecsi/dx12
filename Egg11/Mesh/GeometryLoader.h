#pragma once

#include "Mesh/Geometry.h"

namespace Egg11 { namespace Mesh
{
	class Geometry;

	class GeometryLoader
	{
		static Mesh::Geometry::P createGeometryFromMemory(ID3D11Device* device, BYTE* data, unsigned int nBytes);

	public:
		static Mesh::Geometry::P createGeometryFromFile(ID3D11Device* device, const char* filename);
	};

}} // namespace Egg11::Mesh