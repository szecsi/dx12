
#define particleCount 1024*2
//#define particleCount 256
#define counterSize 3

//#define controlParticleCount 1024 * 4

//#define controlParticleCount 1024 * 3

//#define controlParticleCount 512


#define boundarySide 0.3
#define boundaryBottom 0.0
#define boundaryTop 1.0

#define mortonBinPerAxis 

struct Particle
{
	float3	position;
	float	massDensity;
	float3	velocity;
	float	pressure;
	float3 temp;
	uint zindex;
};

struct ControlParticle
{
	float3	position;
	float	controlPressureRatio;
	float3	nonAnimatedPos;
	float	temp;
	float4	blendWeights;
	uint4	blendIndices;
};

