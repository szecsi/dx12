#include "billboardTree.hlsli"

// xyz = ground position, w = per-instance size multiplier
Buffer<float4> billboardPositions : register(t0);

static const uint billboardViewCount = 8;      // must match TreeApp::billboardViewCount
static const float billboardWorldSize = 16.0;  // must match TreeApp::billboardWorldSize

[RootSignature(BillboardTreeRootSig)]
VSOutput billboardTreeVS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {
	float4 inst = billboardPositions[instanceId];
	float3 worldPos = inst.xyz;
	float scale = inst.w;

	float3 toCam = cameraPos.xyz - worldPos;
	toCam.y = 0.0;
	float toCamLen = length(toCam);
	float3 fwd = (toCamLen > 0.0001) ? (toCam / toCamLen) : float3(0.0, 0.0, 1.0);
	float3 right = normalize(cross(fwd, float3(0.0, 1.0, 0.0)));

	// vertexId -> triangle-strip quad corner: 0=(-1,0) 1=(1,0) 2=(-1,1) 3=(1,1)
	float2 corner = float2((vertexId & 1) ? 1.0 : -1.0, (vertexId & 2) ? 1.0 : 0.0);

	float3 worldCorner = worldPos
		+ right * corner.x * (billboardWorldSize * 0.5) * scale
		+ float3(0.0, 1.0, 0.0) * corner.y * billboardWorldSize * scale;

	VSOutput vso;
	vso.position = mul(viewProjMat, float4(worldCorner, 1.0));

	// pick the baked view whose camera angle best matches the real tree->camera direction
	float theta = atan2(fwd.x, fwd.z);
	if (theta < 0.0) theta += 6.28318530718;
	uint tile = (uint)round(theta / (6.28318530718 / billboardViewCount)) % billboardViewCount;

	vso.texCoord = float2((tile + corner.x * 0.5 + 0.5) / billboardViewCount, 1.0 - corner.y);
	return vso;
}
