#include "QuadricFootField.hlsli"

#define TransportTensorSig "RootFlags(0)," \
    "CBV(b0)," \
    "SRV(t0)," \
    "SRV(t2)," \
    "UAV(u1)"

// Pass 2: pre-rotation state (old normals) and post-rotation state (new
// normals, from CSNormalRotation.hlsl) for every slot.
StructuredBuffer<PackedNode4> CurrentNodes : register(t0);
StructuredBuffer<PackedNode4> NormalUpdatedNodes : register(t2);
RWStructuredBuffer<PackedNode4> TransportedNodes : register(u1);

// Rotates each populated slot's curvature tensor A to follow its (possibly
// rotated) normal -- the minimal rotation taking the old normal to the new
// one -- then re-projects A so the new normal is (approximately) one of its
// eigenvectors, keeping the tangential curvature description meaningful
// after the normal moved. No per-slot label/neighbor lookups needed here,
// so an empty (SENTINEL_LABEL) slot is harmless to process uniformly: its
// normal never moves (CSNormalRotation.hlsl skips it), so old==new, R is the
// identity, and its already-zero A stays zero.
[RootSignature(TransportTensorSig)]
[numthreads(THREADS_X, 1, 1)]
void CSTransportTensor(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    uint totalCount = 2u * LatticeNodeCount;
    if (index >= totalCount)
        return;

    PackedNode4 oldPacked = CurrentNodes[index];
    PackedNode4 newPacked = NormalUpdatedNodes[index];
    PackedNode4 outPacked = newPacked;

    [unroll]
    for (uint slot = 0; slot < SparseLabelCount; slot++)
    {
        LabelSlotState oldState = DecodeSlot(oldPacked, slot);
        LabelSlotState newState = DecodeSlot(newPacked, slot);

        float3x3 R = MinimalRotation(oldState.normal, newState.normal);
        float3x3 transported = mul(R, mul(oldState.A, transpose(R)));
        newState.A = ClampFrobenius(ProjectTensorToNormal(transported, newState.normal));
        EncodeSlot(outPacked, slot, newState);
    }

    TransportedNodes[index] = outPacked;
}
