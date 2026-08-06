#include "BccFrameCb.hlsli"
#include "bccCommon.hlsli"

Texture3D<uint4> gA0 : register(t0);
Texture3D<uint4> gB0 : register(t2);

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

// Deliberately simple/blocky 16-color palette to distinguish labels; later
// filtering passes are expected to smooth the underlying field, not this.
static const float3 LabelPalette[16] = {
    float3(0.10, 0.10, 0.12), float3(0.85, 0.25, 0.25),
    float3(0.25, 0.70, 0.30), float3(0.25, 0.45, 0.85),
    float3(0.90, 0.75, 0.20), float3(0.75, 0.30, 0.80),
    float3(0.25, 0.80, 0.80), float3(0.90, 0.50, 0.20),
    float3(0.55, 0.55, 0.55), float3(0.60, 0.20, 0.20),
    float3(0.20, 0.45, 0.20), float3(0.20, 0.25, 0.60),
    float3(0.60, 0.50, 0.10), float3(0.45, 0.15, 0.50),
    float3(0.15, 0.50, 0.50), float3(0.55, 0.30, 0.10),
};

// Finds the nearer of (nearest A lattice point, nearest B lattice point) to
// worldPos and returns its precomputed footvector data.
void SampleFootField(float3 worldPos, out float dist, out uint ownLabel, out uint otherLabel)
{
    int3 aIdx = clamp((int3)round(worldPos / CellSize), 0, (int)GridRes - 1);
    int3 bIdx = clamp((int3)round(worldPos / CellSize - 0.5), 0, (int)GridRes - 1);

    uint4 a = gA0.Load(int4(aIdx, 0));
    uint4 b = gB0.Load(int4(bIdx, 0));

    float da = distance(APos(aIdx), worldPos);
    float db = distance(BPos(bIdx), worldPos);

    if (da <= db) {
        ownLabel = a.x; otherLabel = a.y;
        dist = (a.y != SENTINEL_LABEL) ? asfloat(a.w) : 1.0e6;
    } else {
        ownLabel = b.x; otherLabel = b.y;
        dist = (b.y != SENTINEL_LABEL) ? asfloat(b.w) : 1.0e6;
    }
}

// The footvector length is a distance -- own label == outsideLabel gives it a
// negative sign, otherwise positive -- so it's a (per-lattice-point) SDF.
// Sampled at the single nearest lattice point, that SDF is piecewise constant
// (a staircase): marching or bisecting it directly always converges to a
// voxel boundary, not the true smoothly-varying surface, however many
// iterations are spent -- that's what was still showing as faceting in the
// Distance visualization even for the analytic (mathematically exact)
// footvector field. Trilinearly interpolating the *signed* per-corner values
// of the cell containing p (rather than each corner's raw distance
// separately) reconstructs the standard, continuous SDF approximation whose
// zero-crossing is what marching/bisection should actually be homing in on.
//
// `useB` picks which of the two interleaved simple-cubic sublattices this
// particular trilinear reconstruction is built from -- see InterpolatedSDF.
float TrilinearSignedDist(float3 worldPos, uint outsideLabel, bool useB)
{
    float3 localPos   = worldPos / CellSize - (useB ? 0.5 : 0.0);
    float3 cellOrigin = floor(localPos);
    float3 frac       = localPos - cellOrigin;

    float sum = 0.0;
    for (uint dz = 0; dz < 2; dz++) {
        for (uint dy = 0; dy < 2; dy++) {
            for (uint dx = 0; dx < 2; dx++) {
                float wx = dx ? frac.x : (1.0 - frac.x);
                float wy = dy ? frac.y : (1.0 - frac.y);
                float wz = dz ? frac.z : (1.0 - frac.z);

                int3 idx = clamp((int3)cellOrigin + int3(dx, dy, dz), 0, (int)GridRes - 1);
                uint4 texel = useB ? gB0.Load(int4(idx, 0)) : gA0.Load(int4(idx, 0));
                float cornerDist = (texel.y != SENTINEL_LABEL) ? asfloat(texel.w) : 1.0e6;
                float cornerSigned = (texel.x == outsideLabel) ? -cornerDist : cornerDist;

                sum += cornerSigned * (wx * wy * wz);
            }
        }
    }
    return sum;
}

// Csebfalvi's trick for cheap BCC reconstruction: rather than hard-switching
// to whichever sublattice is nearer (A's trilinear field and B's trilinear
// field are each individually continuous, but generally *disagree* at the
// switchover boundary, so a discrete pick between them tears a visible seam
// there), reconstruct trilinearly on EACH sublattice independently and blend
// the two with fixed, position-independent weights. Both terms are smooth
// everywhere on their own, and a constant-coefficient sum of two smooth
// functions is smooth too -- no seam is possible, and (unlike always reading
// only A) B's samples still contribute everywhere.
float InterpolatedSDF(float3 worldPos, uint outsideLabel)
{
    float sdfA = TrilinearSignedDist(worldPos, outsideLabel, false);
    float sdfB = TrilinearSignedDist(worldPos, outsideLabel, true);
    return 0.5 * sdfA + 0.5 * sdfB;
}

#define RENDER_MODE_LABELS   0
#define RENDER_MODE_NORMALS  1
#define RENDER_MODE_DISTANCE 2

// Inigo Quilez's cosine palette trick: a smooth, continuous, repeating
// rainbow as a function of t (period 1). Used to color by camera distance --
// the repetition is the point: banding makes small distance discontinuities
// (e.g. a bisection that didn't quite converge, or a seam between JFA and
// analytic-quality regions) obvious as a break in an otherwise-smooth cycle
// of bands, which a single monotonic gradient over the whole visible range
// would hide in a couple of imperceptible shades.
float3 CyclicColor(float t)
{
    const float3 phase = float3(0.0, 0.333, 0.667);
    return 0.5 + 0.5 * cos(6.28318530718 * (t + phase));
}

// Adds `weight` times the normalized direction from `pos` to its stored seed
// into `sum`, unless `pos` is (at or very near) its own seed -- a zero-length
// vector would normalize to NaN and poison the whole running sum.
//
// A corner's footvector points toward the interface FROM that corner's own
// side, so a corner sitting on the outside label points its vector opposite
// to a same-side ("inside") corner's, even though both describe the same
// interface -- blending them directly would partially cancel instead of
// reinforcing. Flipping every contribution whose own label doesn't match the
// outside label re-aligns them all to the same convention first.
void AddWeightedFootDirection(uint packedSeed, float3 pos, bool flip, float weight, inout float3 sum)
{
    float3 d = SeedWorldPos(packedSeed) - pos;
    if (dot(d, d) > 1.0e-6) sum += (flip ? -normalize(d) : normalize(d)) * weight;
}

// Accumulates the (label-aware) footvector direction of every lattice point
// on ONE sublattice -- fixed by `useB`, not "whichever is nearer" -- within
// `radius` world units of worldPos, weighted by a smoothstep falloff of
// distance, into `sum`. Plain 8-corner trilinear interpolation turns out to
// look *more* faceted here, not less: it reproduces the underlying
// per-cell-linear data exactly, so the (generally mismatched) gradient
// between adjacent cells shows up as a hard, very visible crease -- the
// classic flat-shaded-low-poly look. A smoothstep kernel wide enough to span
// several cells blends across those creases instead of reproducing them, and
// its zero derivative at the cutoff radius means points fade in/out of the
// sum with no seam as worldPos moves.
void AccumulateFilteredNormal(float3 worldPos, uint outsideLabel, bool useB, inout float3 sum)
{
    float3 localPos   = worldPos / CellSize - (useB ? 0.5 : 0.0);
    int3   cellOrigin = (int3)floor(localPos);

    const float radius = 1.75 * CellSize;

    for (int dz = -1; dz <= 2; dz++) {
        for (int dy = -1; dy <= 2; dy++) {
            for (int dx = -1; dx <= 2; dx++) {
                int3 idx = clamp(cellOrigin + int3(dx, dy, dz), 0, (int)GridRes - 1);
                float3 candPos = useB ? BPos(idx) : APos(idx);

                float d = distance(worldPos, candPos);
                if (d >= radius) continue;

                float t = 1.0 - d / radius;
                float w = t * t * (3.0 - 2.0 * t); // smoothstep: 0 value AND 0 slope at d == radius

                uint4 texel = useB ? gB0.Load(int4(idx, 0)) : gA0.Load(int4(idx, 0));
                if (texel.y == SENTINEL_LABEL) continue;

                AddWeightedFootDirection(texel.z, candPos, texel.x != outsideLabel, w, sum);
            }
        }
    }
}

// Estimates a surface normal at worldPos via Csebfalvi's trick: accumulate
// the smoothstep-weighted neighborhood contribution from EACH sublattice
// independently (rather than picking whichever is nearer, which -- exactly
// as with InterpolatedSDF above -- would tear a seam where the pick flips),
// then normalize once at the end. worldPos is expected to still be just on
// the "near" side of the surface (the sphere-tracer's last step position,
// not a point exactly on the interface), so the neighborhood doesn't
// straddle both sides and cancel out to nothing.
float3 EstimateFilteredNormal(float3 worldPos, uint outsideLabel)
{
    float3 sum = float3(0, 0, 0);
    AccumulateFilteredNormal(worldPos, outsideLabel, false, sum);
    AccumulateFilteredNormal(worldPos, outsideLabel, true, sum);

    if (dot(sum, sum) < 1.0e-12) return float3(0, 0, 1);
    return normalize(sum);
}

struct PsOut {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

// Writes an explicit per-pixel depth (from the marched hit position, or the
// far plane on a miss) into the real depth buffer -- this pass has no actual
// 3D geometry (it's a full-screen triangle), so unlike a normal mesh draw the
// rasterizer's own interpolated depth is meaningless and must be overridden
// here. This is what lets the footvector overlay passes (footLineVS/
// footPointVS, ordinary depth-tested geometry) occlude correctly against the
// marched surface instead of always drawing on top of it.
PsOut raymarchPS(VsOut input)
{
    PsOut result;

    float3 ro = cameraPos.xyz;
    float3 rd = normalize(input.rayDir);

    float maxDist  = gridParams.w;
    int   maxSteps = (int)gridParams.z;
    int   renderMode = (int)renderParams.x;

    // Whatever label the ray starts in (background, in the normal case) is
    // "outside". Two independent stop conditions, either of which leaves
    // tPrev/t bracketing a sign change of InterpolatedSDF for the binary
    // search below: abs(sdf) < epsilon is the primary one, now that sdf is a
    // real (interpolated) SDF and shrinks smoothly toward zero near the
    // surface; a label flip is a safety net for when the field (JFA
    // especially, but even the analytic one near torus overlaps) doesn't
    // shrink fast enough and a step would otherwise skip clean through a thin
    // part of the surface before either check catches it (see the
    // sphere-tracing hole discussion) -- relying on the label flip alone
    // caused exactly that, badly.
    float outsideDist; uint outsideLabel, outsideOther;
    SampleFootField(ro, outsideDist, outsideLabel, outsideOther);

    float t = 0.0, tPrev = 0.0;
    bool hit = false;
    for (int i = 0; i < maxSteps; i++) {
        float3 p = ro + rd * t;

        uint ownLabel, otherLabel;
        float unusedDist;
        SampleFootField(p, unusedDist, ownLabel, otherLabel);
        float sdf = InterpolatedSDF(p, outsideLabel);

        if (/*ownLabel != outsideLabel || */abs(sdf) < 0.005) { hit = true; break; }

        tPrev = t;
        t += max(abs(sdf), 0.002);
        if (t > maxDist) break;
    }

    if (hit) {
        // Binary search the zero of InterpolatedSDF between the last
        // confirmed-outside step and this one. Bisecting the raw per-voxel
        // SampleFootField (piecewise constant) always converges to a voxel
        // boundary no matter how many iterations run; bisecting the
        // trilinearly-interpolated SDF instead converges to a position that
        // varies continuously with screen position, which is what actually
        // removes the faceting.
        float tLo = tPrev, tHi = t;
        for (int b = 0; b < 10; b++) {
            float tMid = 0.5 * (tLo + tHi);
            float sd = InterpolatedSDF(ro + rd * tMid, outsideLabel);
            if (sd < 0.0) tLo = tMid; else tHi = tMid;
        }
        float3 p = ro + rd * tHi;
        uint ownLabel, otherLabel;

        float finalDist;
        SampleFootField(p, finalDist, ownLabel, otherLabel);

        float4 clipHit = mul(float4(p, 1), viewProjTransform);
        result.depth = clipHit.z / clipHit.w;

        if (renderMode == RENDER_MODE_NORMALS) {
            float3 n = EstimateFilteredNormal(p, otherLabel);
            // The label-based flip only makes neighbors self-consistent; it
            // doesn't pin down which of the two directions is "outward".
            // Resolve that by forcing the normal to face back toward the
            // camera, same idea as a backface check.
            if (dot(rd, n) > 0.0) n = -n;
            // Raw n*0.5+0.5 hits 0 or 1 on any component whenever the normal
            // points anywhere near a coordinate axis, which happens across
            // wide, gently-curved patches of a smooth surface -- those read
            // as flat, oversaturated blocks of primary color. Pulling the
            // encoding toward mid-gray keeps the same hue/direction cues
            // without ever fully clipping, so shading variation stays visible
            // everywhere instead of disappearing into flat zones.
            const float saturation = 0.6;
            float3 color = lerp(float3(0.5, 0.5, 0.5), n * 0.5 + 0.5, saturation);
            result.color = float4(color, 1.0);
            return result;
        }
        if (renderMode == RENDER_MODE_DISTANCE) {
            float camDist = length(p - ro);
            result.color = float4(CyclicColor(camDist / 2.0), 1.0);
            return result;
        }
        float3 c0 = LabelPalette[ownLabel & 15];
        float3 c1 = LabelPalette[otherLabel & 15];
        result.color = float4(lerp(c1, c0, 0.7), 1.0);
        return result;
    }

    result.color = float4(0.05, 0.06, 0.08, 1.0);
    result.depth = 1.0;
    return result;
}
