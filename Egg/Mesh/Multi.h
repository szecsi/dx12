#pragma once

#include "../Common.h"
#include "Flip.h"
#include <map>

namespace Egg {
	namespace Mesh {

		GG_CLASS(Multi)
			std::vector<Egg::Mesh::Flip::P> submeshes;
		protected:
			Multi(){}
		public:
			void Add(Egg::Mesh::Flip::P flip) {
				submeshes.push_back(flip);
			}

			Egg::Mesh::Flip::P GetSubmesh(unsigned int index) {
				return submeshes.at(index);
			}

			Egg::Mesh::Geometry::P GetGeometry(unsigned int mien, unsigned int index) {
				return submeshes.at(index)->GetShaded(mien)->GetGeometry();
			}

			void SetTopology(D3D_PRIMITIVE_TOPOLOGY topo) {
				for (auto& i : submeshes) {
					i->SetTopology(topo);
				}
			}

			void Draw(ID3D12GraphicsCommandList* commandList, unsigned int mien, unsigned int objectIndex = 0) {
				for (auto& i : submeshes) {
					i->Draw(commandList, mien, objectIndex);
				}
			}
		GG_ENDCLASS
	}
}