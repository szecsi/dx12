#include "matrix.hlsli"
#include "tree.hlsli"
#include "treeCB.hlsli"

#define GrowSig "RootFlags( 0 )," \
    "CBV(b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=3))"

RWByteAddressBuffer pieces     : register(u0);
RWBuffer<float4>    bones      : register(u1);
RWByteAddressBuffer twists     : register(u2);

groupshared Mat4 sharedBoneMats[256];
//groupshared int sharedTwists[128];

[RootSignature(GrowSig)]
[numthreads(256, 1, 1)]
void growCS( uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID )
{
    uint modelIndex = gid.x;
    uint modelOffset = modelIndex * 256;
    uint boneOffset = modelOffset * 2;
    uint nPiecesThisModel = 255;//nextModelOffset - modelOffset;
    if (tid.x == 0)
    {
        Mat4 root;
        root.m = float4x4(
          1, 0, 0, 0,
          0, 1, 0, 0,
          0, 0, 1, 0,
          0, 0, 0, 1
        );
        sharedBoneMats[0] = root;
        bones [boneOffset*4+0] = float4(1, 0, 0, 0);
        bones [boneOffset*4+1] = float4(0, 1, 0, 0);
        bones [boneOffset*4+2] = float4(0, 0, 1, 0);
        bones [boneOffset*4+3] = float4(0, 0, 0, 1);
     }
    /*
        0+1=1
        1+2=3
        3+4=7 
        7+8=15
        15+16=31
        31+32=63
        63+64=127
        127+128=255
    */
    int rangeStart = 0;
    int rangeLength = 1;
    float dimi = 0.7;
    while(rangeLength < 129){
        GroupMemoryBarrierWithGroupSync();
        uint iBone = rangeStart + tid.x;            
        if (tid.x < (uint)rangeLength && iBone < nPiecesThisModel)
        {
            uint b1 = iBone * 2 + 1;
            uint b2 = iBone * 2 + 2;
            pieces.Store((modelOffset * 4 + iBone * 4 + 0) * 4, iBone);
            pieces.Store((modelOffset * 4 + iBone * 4 + 1) * 4, b1);
            pieces.Store((modelOffset * 4 + iBone * 4 + 2) * 4, b2);
            pieces.Store((modelOffset * 4 + iBone * 4 + 3) * 4, -1);
            
            int ptwist = (iBone * 11u + modelIndex * 13u) % 6u;
            twists.Store((modelOffset + iBone) * 4, asuint(ptwist));

            float twistAngle = ptwist * (6.28318530718 / 6.0) + stripWidth * 0.0001;

            Mat4 mb2 =
                RotationZ(sin(time*0.45) * 0.26 - (int)((b2 * 17 + modelIndex * 19) % 9) * (0.52 / 9.0)) *
//                Scale(0.707, 0.707, 0.707) *
                Scale(dimi, dimi, dimi) *                
                //RotationY(-0.785f) *
                //RotationY(sin(time*1.1111)*(-0.3) - (int)((b2 * 23 + modelIndex * 29) % 9) * (0.8 / 9.0)) *
                RotationY(-0.785f + 0.5 * sin(time*1.1)) *
                Translation(-1.5, 0, 3) *                
                RotationZ(twistAngle) *                
                sharedBoneMats[iBone]
                ;
            if(b2 < 256)
                sharedBoneMats[b2] = mb2;
            bones [(boneOffset + b2)*4+0] = mb2[0];
            bones [(boneOffset + b2)*4+1] = mb2[1];
            bones [(boneOffset + b2)*4+2] = mb2[2];
            bones [(boneOffset + b2)*4+3] = mb2[3];

            Mat4 mb1 =
                RotationZ(sin(time) * 0.26 - (int)((b1 * 31 + modelIndex * 33) % 9) * (0.52 / 9.0)) *
  //              Scale(0.707, 0.707, 0.707) *
                Scale(dimi, dimi, dimi) *
                //RotationY(0.785f) *
                //RotationY( 0.3 + (int)((b1 * 37 + modelIndex * 41) % 9) * (0.8 / 9.0)) *
                RotationY(0.785f + 0.5 * sin(time*1.3)) * 
                Translation(1.5, 0, 3) *                
                RotationZ(twistAngle) *
                sharedBoneMats[iBone]
                ;
            if(b1 < 256)
                sharedBoneMats[b1] = mb1;
            bones [(boneOffset + b1)*4+0] = mb1[0];
            bones [(boneOffset + b1)*4+1] = mb1[1];
            bones [(boneOffset + b1)*4+2] = mb1[2];
            bones [(boneOffset + b1)*4+3] = mb1[3];

        }
        dimi += 0.025;
        rangeStart += rangeLength;
        rangeLength <<= 1;
    }
}


/*
now we have pieces, bones, twists
bones are global and shared between all kinds of pieces
pieces and twists are per pieces, separated by piece type
twist is just 3 bits, can be folded in pieces

let's give every bone a piece type, bzw a rank, using some hash function
let's switch from 'to children' to 'from parent'
need to find parent
    we know where we are in row
    if we have a scan of the previous ranks, we would have to search for the number there
    after scan, we scatter into bitfield
    use countbits to find parent index on previous row
    scan would need to be 256 wide (8 waves)
    bitfield would need to be 256 wide (8 ints)

*/