#pragma once
#include "../Common.h"
#include "physx/PxPhysicsAPI.h"
#include <vector>

namespace Egg {
	namespace Physics
	{
		GG_CLASS(Material)

			physx::PxMaterial* material;
		protected:
			Material(physx::PxMaterial* material):
			material(material){
			}
		public:
			physx::PxMaterial* GetPxMaterial() {
				return material;
			}

		GG_ENDCLASS

	}
}
