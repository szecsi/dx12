
#define particleCount 1024*4
#define particlePerCore 128
//#define particleCount 256
#define counterSize 3

//#define controlParticleCount 1024 * 4

//#define controlParticleCount 1024 * 3

//#define controlParticleCount 512

#define boundarySide_Fluid 0.3
#define boundaryBottom_Fluid 0.1
#define boundaryTop_Fluid 1.0

#define boundarySide 0.4
#define boundaryBottom 0.0
#define boundaryTop 1.0

struct ControlParticle
{
	float3	position;
	float	controlPressureRatio;
	float3	nonAnimatedPos;
	float	temp;
	float4	blendWeights;
	uint4	blendIndices;
};

