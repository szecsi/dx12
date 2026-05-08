#define RootSigRetam "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), CBV(b0), CBV(b1), CBV(b2), DescriptorTable(SRV(t0, numDescriptors=1)), StaticSampler(s0)"
#define RootSigRetamCollect "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), CBV(b0), CBV(b1), CBV(b2), DescriptorTable(UAV(u0, numDescriptors=2)), DescriptorTable(SRV(t0, numDescriptors=1)), StaticSampler(s0)"
#define RootSigRetamSkinned "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), CBV(b0), CBV(b1), CBV(b2), CBV(b3), DescriptorTable(SRV(t0, numDescriptors=1)), StaticSampler(s0)"
#define RootSigRetamSkinnedCollect "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), CBV(b0), CBV(b1), CBV(b2), CBV(b3), DescriptorTable(UAV(u0, numDescriptors=2)), DescriptorTable(SRV(t0, numDescriptors=1)), StaticSampler(s0)"
