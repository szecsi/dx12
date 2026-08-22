#pragma once

// PCG-style integer hash / float01 RNG. No prior hash utility exists
// anywhere in this codebase (checked), so this is new.
uint PcgHash(uint x) {
    uint s = x * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}

float PcgFloat01(uint x) {
    return float(PcgHash(x)) / 4294967296.0;
}

uint HashCombine(uint a, uint b, uint c) {
    return PcgHash(a ^ PcgHash(b ^ PcgHash(c * 0x9E3779B9u)));
}
