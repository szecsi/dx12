#pragma once

#include "../Common.h"
#include "Shaded.h"
#include <map>

namespace Egg {
	namespace Mesh {

		GG_CLASS(Flip)
			std::map<unsigned int, Shaded::P> mienShadedMap;
		protected:
			Flip() {}
		public:
			void Add(unsigned int mien, Shaded::P shaded) {
				auto i = mienShadedMap.find(mien);
				if (i != mienShadedMap.end())
					mienShadedMap.erase(i);
				mienShadedMap[mien] = shaded;
			}

			Shaded::P GetShaded(unsigned int mien)
			{
				return mienShadedMap[mien];
			}

			void SetTopology(D3D_PRIMITIVE_TOPOLOGY topo) {
				for (auto& i : mienShadedMap) {
					i.second->SetTopology(topo);
				}
			}

			void Draw(ID3D12GraphicsCommandList* commandList, unsigned int mien, unsigned int objectIndex = 0) {
				auto i = mienShadedMap.find(mien);
				if (i != mienShadedMap.end()) {
					i->second->Draw(commandList, objectIndex);
				}
			}
		GG_ENDCLASS
	}
}