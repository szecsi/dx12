#pragma once

#include "bccCommon.hlsli"

// Multiphase label-potential filtering (v1: diffusion only, see the
// "Multiphase potential filtering" plan). Each B lattice point's single
// (ownLabel, otherLabel, distance) is temporarily re-expressed as one
// potential phi_l per label; the winning label is argmax(phi), and the
// interface between any two labels a/b is the zero set of phi_a - phi_b.
// Deriving every pairwise interface from the same shared potentials is what
// keeps triple-junction behavior consistent -- unlike the existing single
// signed-distance representation, which can only describe "the" nearest
// interface, not "which of several competing labels is nearest".
//
// K is fixed at 4 (background + the 3 test torii in BccApp::BuildTorusList)
// -- not a general mechanism, so every helper below is written as 4 explicit
// .x/.y/.z/.w branches rather than a dynamically-indexed loop.
static const uint K = 4;

// Potentials are stored 8 bits each, packed into a single uint (one texture
// channel) -- "8 bits per potential" is enough because only argmax() and
// small differences near interfaces matter, not absolute precision far from
// any boundary. SCALE_PER_UNIT/CENTER map a world-distance range onto the
// 8-bit range; it must comfortably cover the *whole grid's* possible
// distances (up to a GridRes-scale diagonal for a point far from every
// torus, not just the ~8-16 units actually shown by the footvector overlay's
// default Max Vector Length) -- an earlier, tighter range (+-16) silently
// saturated most of the grid to a flat clipped value, collapsing
// grad(phi_a-phi_b) to near zero almost everywhere and making the footvector
// recovery formula (see labelPotentialRecoverCS.hlsl) divide by that
// near-zero gradient, scattering recovered footpoints essentially at random.
// +-63.5 at 0.5-unit resolution comfortably covers this GridRes=48 grid with
// margin.
static const float POTENTIAL_SCALE_PER_UNIT = 2.0;
static const float POTENTIAL_CENTER = 128.0;

float DecodePotential(uint byteVal)
{
    return (float(byteVal) - POTENTIAL_CENTER) / POTENTIAL_SCALE_PER_UNIT;
}

uint EncodePotential(float phi)
{
    return (uint)clamp(round(phi * POTENTIAL_SCALE_PER_UNIT) + POTENTIAL_CENTER, 0.0, 255.0);
}

// The most-negative encodable value -- used as "this label is not a
// plausible candidate here", the multiphase analogue of the existing
// SENTINEL_LABEL/1.0e6 conventions elsewhere in this codebase.
static const float POTENTIAL_MIN = (0.0 - POTENTIAL_CENTER) / POTENTIAL_SCALE_PER_UNIT; // == DecodePotential(0)

uint PackPotentials4(float4 phi)
{
    uint b0 = EncodePotential(phi.x);
    uint b1 = EncodePotential(phi.y);
    uint b2 = EncodePotential(phi.z);
    uint b3 = EncodePotential(phi.w);
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

float4 UnpackPotentials4(uint packed)
{
    return float4(
        DecodePotential(packed & 0xFFu),
        DecodePotential((packed >> 8) & 0xFFu),
        DecodePotential((packed >> 16) & 0xFFu),
        DecodePotential((packed >> 24) & 0xFFu));
}

void SetLabelPotential(inout float4 phi, uint label, float value)
{
    if (label == 0) phi.x = value;
    else if (label == 1) phi.y = value;
    else if (label == 2) phi.z = value;
    else if (label == 3) phi.w = value;
}

float GetLabelPotential(float4 phi, uint label)
{
    if (label == 0) return phi.x;
    if (label == 1) return phi.y;
    if (label == 2) return phi.z;
    return phi.w;
}

// Converts an existing JFA/analytic-format texel (ownLabel, otherLabel,
// packedSeed, asuint(dist)) into a "virtual" potential vector: +dist on the
// own-label channel, -dist on the other-label channel (if any), POTENTIAL_MIN
// (== "not a candidate") on every other label. Used both to seed B's own
// potentials (labelPotentialInitCS.hlsl) and to convert each fixed A0
// neighbor on the fly during diffusion (labelPotentialDiffuseCS.hlsl) --
// A never needs its own stored potentials this way.
float4 TexelToPotentials(uint4 texel)
{
    float4 phi = float4(POTENTIAL_MIN, POTENTIAL_MIN, POTENTIAL_MIN, POTENTIAL_MIN);
    float dist = (texel.y != SENTINEL_LABEL) ? asfloat(texel.w) : 1.0e6;
    SetLabelPotential(phi, texel.x, dist);
    if (texel.y != SENTINEL_LABEL)
        SetLabelPotential(phi, texel.y, -dist);
    return phi;
}

// True if `texel` (a raw JFA/analytic B0-format texel, own/other label pair)
// actually carries real distance information for `label` -- i.e. `label` is
// this texel's own label or its recorded nearest-other label. A texel only
// ever remembers ONE "other" label; any of the remaining K-2 labels gets a
// flat POTENTIAL_MIN out of TexelToPotentials() regardless of how close that
// label's real surface might be there. That's a fine standalone meaning
// ("not locally dominant"), but it is NOT real per-neighbor gradient data --
// see EstimatePsiGradient in labelPotentialRecoverCS.hlsl, which uses this to
// detect when a finite-difference estimate would be comparing fabricated
// values instead of two labels' actual local distances.
bool LabelKnownInTexel(uint4 texel, uint label)
{
    if (texel.x == label) return true;
    if (texel.y == label && texel.y != SENTINEL_LABEL) return true;
    return false;
}

uint ArgmaxLabel4(float4 phi)
{
    uint best = 0;
    float bestVal = phi.x;
    if (phi.y > bestVal) { bestVal = phi.y; best = 1; }
    if (phi.z > bestVal) { bestVal = phi.z; best = 2; }
    if (phi.w > bestVal) { bestVal = phi.w; best = 3; }
    return best;
}

// Second-highest label, excluding `exclude`.
uint SecondArgmaxLabel4(float4 phi, uint exclude)
{
    uint best = (exclude == 0) ? 1 : 0;
    float bestVal = GetLabelPotential(phi, best);
    for (uint l = best + 1; l < K; l++) {
        if (l == exclude) continue;
        float v = GetLabelPotential(phi, l);
        if (v > bestVal) { bestVal = v; best = l; }
    }
    return best;
}
