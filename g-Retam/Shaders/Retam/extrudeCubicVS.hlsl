#define CubicExtrudeSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "DescriptorTable(SRV(t0, numDescriptors=1))"

[RootSignature(CubicExtrudeSig)]
void extrudeCubicVS() { }
