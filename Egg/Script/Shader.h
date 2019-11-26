#pragma once

#include "../Common.h"

namespace Egg {
	namespace Script {
		GG_CLASS(Shader)
		public:
			com_ptr<ID3DBlob> byteCode;
			com_ptr<ID3D12RootSignature> rootSig;
		GG_ENDCLASS
	}
}