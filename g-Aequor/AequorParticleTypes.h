#pragma once

// SoA (Structure of Arrays) particle field definitions -- own enum, not
// pbf's ParticleTypes.h: g-Aequor particles carry no velocity/density/omega,
// only what the surface-relaxation constraints need. Field order matches the
// UAV register order SpatialGrid.h binds them in, and permutateCS.hlsl's
// sort-scatter order.
enum AequorParticleField : unsigned int {
	PF_POSITION = 0,      // float3: current world-space position
	PF_NORMAL,             // float3: outward unit normal
	PF_TENSOR_DIAG,        // float3: (Axx, Ayy, Azz) of the symmetric curvature tensor A
	PF_TENSOR_OFFDIAG,     // float3: (Axy, Axz, Ayz)
	PF_LABEL,               // uint:  SENTINEL_LABEL for inactive slots
	PF_COUNT
};

static constexpr unsigned int aequorFieldStrides[PF_COUNT] = {
	sizeof(float) * 3, // PF_POSITION
	sizeof(float) * 3, // PF_NORMAL
	sizeof(float) * 3, // PF_TENSOR_DIAG
	sizeof(float) * 3, // PF_TENSOR_OFFDIAG
	sizeof(unsigned int), // PF_LABEL
};

static constexpr const wchar_t* aequorFieldNames[PF_COUNT] = {
	L"Position", L"Normal", L"TensorDiag", L"TensorOffdiag", L"Label"
};
