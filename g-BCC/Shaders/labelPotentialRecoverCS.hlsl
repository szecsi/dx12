#include "bccCommon.hlsli"
#include "LabelPotential.hlsli"

#define LabelPotentialRecoverSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))"

cbuffer RootConsts : register(b0) {
    uint finalIsA1; // which of A1/B1 holds the converged potentials -- mirrors jfaFinalizeCS.hlsl's finalPingIndex
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);
RWTexture3D<uint4> gBFoot : register(u5);

// Estimates grad(phi_a - phi_b) at a B lattice point from its 8 fixed
// BCC-nearest A-neighbors (CrossOffsetsBtoA -- exactly the 8 corners of the
// unit cube centered on this B point, same structure TrilinearSignedDist in
// raymarchPS.hlsl uses for its corner blend). The standard finite-difference
// gradient of a trilinear field evaluated at a cube's center reduces to: for
// each axis, the average of the 4 corners on the positive side minus the
// average of the 4 corners on the negative side (divided by CellSize to
// convert from unit-cube to world-space derivative) -- corner order matches
// CrossOffsetsBtoA exactly: index bit0=dx, bit1=dy, bit2=dz.
//
// `allKnown` reports whether every one of the 8 corners actually carries real
// distance data for BOTH labelA and labelB (see LabelKnownInTexel) -- a raw
// JFA/analytic texel only ever remembers ONE "other" label, so a corner
// whose own/other pair doesn't include labelA or labelB contributes a
// fabricated flat POTENTIAL_MIN for it instead of that label's real local
// distance. Mixing real values from some corners with fabricated ones from
// others manufactures a spurious "gradient" pointing at whatever arbitrary
// level POTENTIAL_MIN sits at -- exactly what was pulling recovered
// footpoints onto the bisector between two labels that both happened to be
// "some neighbor's other label" near a triple-ish competition, regardless of
// whether that bisector was this point's real interface at all.
float3 EstimatePsiGradient(uint3 tid, uint labelA, uint labelB, out bool allKnown)
{
    float psiCorner[8];
    allKnown = true;
    for (uint j = 0; j < 8; j++) {
        int3 nIdx = clamp((int3)tid + CrossOffsetsBtoA[j], 0, (int)GridRes - 1);
        uint4 cornerTexel = gA0[nIdx];
        if (!LabelKnownInTexel(cornerTexel, labelA) || !LabelKnownInTexel(cornerTexel, labelB))
            allKnown = false;
        float4 nPhi = TexelToPotentials(cornerTexel);
        psiCorner[j] = GetLabelPotential(nPhi, labelA) - GetLabelPotential(nPhi, labelB);
    }

    float gradX = ((psiCorner[1] + psiCorner[3] + psiCorner[5] + psiCorner[7])
                 - (psiCorner[0] + psiCorner[2] + psiCorner[4] + psiCorner[6])) * 0.25 / CellSize;
    float gradY = ((psiCorner[2] + psiCorner[3] + psiCorner[6] + psiCorner[7])
                 - (psiCorner[0] + psiCorner[1] + psiCorner[4] + psiCorner[5])) * 0.25 / CellSize;
    float gradZ = ((psiCorner[4] + psiCorner[5] + psiCorner[6] + psiCorner[7])
                 - (psiCorner[0] + psiCorner[1] + psiCorner[2] + psiCorner[3])) * 0.25 / CellSize;

    return float3(gradX, gradY, gradZ);
}

// Converts the converged per-label potentials back into the existing
// (ownLabel, otherLabel, packedSeed, asuint(dist)) B0 texel format plus the
// exact-foot-point gBFoot texture (added earlier for analytic mode) -- so
// raymarchPS.hlsl and footVectorBuildCS.hlsl need no changes at all to
// consume filtered output.
[RootSignature(LabelPotentialRecoverSig)]
[numthreads(4, 4, 4)]
void labelPotentialRecoverCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    float4 selfPhi;
    if (finalIsA1 != 0) selfPhi = UnpackPotentials4(gA1[tid].x);
    else                 selfPhi = UnpackPotentials4(gB1[tid].x);

    uint a = ArgmaxLabel4(selfPhi);
    uint b = SecondArgmaxLabel4(selfPhi, a);
    float psi = GetLabelPotential(selfPhi, a) - GetLabelPotential(selfPhi, b);

    bool cornersKnown;
    float3 grad = EstimatePsiGradient(tid, a, b, cornersKnown);
    float  gradLenSq = dot(grad, grad);

    // f = -psi*grad/|grad|^2 is only a sound reconstruction where a real,
    // well-defined interface passes nearby -- there, psi crosses zero with a
    // real slope, estimated from 8 neighbors that all actually know both
    // competing labels' distances (cornersKnown). Anywhere that's not true,
    // or the field is locally flat (grad collapses toward zero), the
    // division just amplifies fabricated/quantization noise into a wild,
    // essentially random vector -- this is what was making recovered
    // footpoints scatter (even landing on the bisector between two unrelated
    // labels near a triple-ish competition) instead of tracking this point's
    // real nearest interface. Detect that and fall back to the existing "no
    // reliable interface here" convention instead (SENTINEL_LABEL / a huge
    // distance) -- footVectorBuildCS.hlsl and raymarchPS.hlsl already treat
    // that correctly (skip it / treat as far).
    const float MIN_RELIABLE_GRAD_LENSQ = 0.01;                // |grad| >= 0.1
    const float MAX_PLAUSIBLE_FOOT_LEN  = GridRes * CellSize;  // longer than the whole grid can't be real

    float3 pos = BPos((int3)tid);
    bool valid = cornersKnown && gradLenSq >= MIN_RELIABLE_GRAD_LENSQ;

    float3 f = float3(0, 0, 0);
    float3 p = pos;
    if (valid) {
        f = -psi * grad / gradLenSq;
        if (length(f) > MAX_PLAUSIBLE_FOOT_LEN) valid = false;
        else p = pos + f;
    }

    if (!valid) {
        gB0[tid] = uint4(a, SENTINEL_LABEL, 0, asuint(1.0e6));
        gBFoot[tid] = uint4(asuint(pos.x), asuint(pos.y), asuint(pos.z), 0);
        return;
    }

    // Quantize the recovered (continuous) foot point to its nearest lattice
    // site -- same convention analyticInitCS.hlsl uses -- so
    // EstimateFilteredNormal's SeedWorldPos(packedSeed) direction lookup
    // keeps working for this init mode too.
    int3 footA = clamp((int3)round(p / CellSize), 0, (int)GridRes - 1);
    int3 footB = clamp((int3)round(p / CellSize - 0.5), 0, (int)GridRes - 1);
    bool footIsB = distance(BPos(footB), p) < distance(APos(footA), p);
    uint packedSeed = PackSeed(footIsB, (uint3)(footIsB ? footB : footA));

    gB0[tid] = uint4(a, b, packedSeed, asuint(length(f)));
    gBFoot[tid] = uint4(asuint(p.x), asuint(p.y), asuint(p.z), 0);
}
