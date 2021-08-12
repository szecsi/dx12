
Buffer<uint> controlParticleCounter;
RWBuffer<uint> indirectDisptach;

[numthreads(1, 1, 1)]
void csSetBufferForIndirectDispatch(uint3 DTid : SV_GroupID)
{	
	indirectDisptach[0] = controlParticleCounter[0];
	indirectDisptach[1] = 1;
	indirectDisptach[2] = 1;
}


