// Wave-intrinsic pixel shader: group-sum values by ID, atomic-add to output buffer.
// Shader Model 6.0+. Assumes wave size <= 32 (firstbitlow on ballot.x only).

Texture2D<uint2> InputTex : register(t0);   // .x = ID,  .y = integer value
RWBuffer<uint>   OutputBuf : register(u0);  // indexed by ID, accumulates sums

float4 main(float4 pos : SV_Position) : SV_Target
{
    // Step 1 — load ID and value from texture
    uint2 sample = InputTex[(uint2)pos.xy];
    uint  myID    = sample.x;
    uint  myValue = sample.y;

    // Steps 2–4 — iterate over unique IDs present in this wave,
    // sum values within each group, and write once per group.
    bool done = false;

    while (WaveActiveAnyTrue(!done))
    {
        // Step 2 — find lanes that share the same ID as the first undone lane
        uint4 undone      = WaveActiveBallot(!done);
        uint  firstUndone = firstbitlow(undone.x);          // lane index of first undone lane
        uint  currentID   = WaveReadLaneAt(myID, firstUndone); // broadcast that lane's ID

        bool  inGroup = !done && (myID == currentID);

        // Step 3 — sum values of all lanes in this group (non-members contribute 0)
        uint groupSum = WaveActiveSum(inGroup ? myValue : 0u);

        if (inGroup)
        {
            done = true;

            // Step 4 — elect one lane per group to issue the atomic add
            uint4 groupMask    = WaveActiveBallot(inGroup);
            uint  firstInGroup = firstbitlow(groupMask.x);
            if (WaveGetLaneIndex() == firstInGroup)
                InterlockedAdd(OutputBuf[currentID], groupSum);
        }
    }

    return float4(0, 0, 0, 0);   // pixel output unused; shader runs for side-effects
}
