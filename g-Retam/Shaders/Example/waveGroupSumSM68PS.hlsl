// SM6.5: WaveMatch, WaveMultiPrefixSum  — eliminates the loop
// SM6.6: float InterlockedAdd           — eliminates the uint-reinterpret hack

Texture2D<uint>  IDTex    : register(t0);
Texture2D<float> ValueTex : register(t1);
globallycoherent RWBuffer<float> OutputBuf : register(u0);  // indexed by ID

float4 main(float4 pos : SV_Position) : SV_Target
{
    uint2 coord = (uint2)pos.xy;

    // Step 1 — load ID and value from textures
    uint  myID    = IDTex[coord];
    float myValue = ValueTex[coord];

    // Step 2 — find all lanes in the wave that share myID (one call, no loop)
    uint4 groupMask = WaveMatch(myID);

    // Step 3 — prefix-sum myValue within the group (current lane excluded)
    float prefixSum = WaveMultiPrefixSum(myValue, groupMask);

    // Elect the last lane in the group: it holds prefixSum of all others,
    // so prefixSum + myValue is the complete group total.
    uint laneIdx   = WaveGetLaneIndex();
    uint groupSize = countbits(groupMask.x) + countbits(groupMask.y); // up to 64-wide waves
    uint laneRank  = laneIdx < 32u
        ? countbits(groupMask.x & ((1u << laneIdx) - 1u))
        : countbits(groupMask.x) + countbits(groupMask.y & ((1u << (laneIdx - 32u)) - 1u));

    // Step 4 — last lane issues one float atomic add per group (SM6.6 native)
    if (laneRank == groupSize - 1u)
        InterlockedAdd(OutputBuf[myID], prefixSum + myValue);

    return float4(0, 0, 0, 0);   // pixel output unused; shader runs for side-effects
}
