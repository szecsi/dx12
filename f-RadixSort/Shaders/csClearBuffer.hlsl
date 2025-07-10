#include "proximity.hlsli"

#define ProxySig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1))"

RWByteAddressBuffer buffer : register(u0);
uint clearValue : register(b0);

[RootSignature(ProxySig)]
[numthreads(rowSize, nRowsPerPage, 1)]
void csClearBuffer( uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
	buffer.Store((tid.x | (tid.y << 5) | (gid.x << 10)) << 2, clearValue);
}