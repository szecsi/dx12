#define CubicExtrudeSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "DescriptorTable(SRV(t0, numDescriptors=1))," \
    "DescriptorTable(SRV(t1, numDescriptors=1))," \
    "CBV(b0)," \
    "StaticSampler(s0)"

[RootSignature(CubicExtrudeSig)]
void extrudeCubicVS() { }
