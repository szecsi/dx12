#ifndef JUNCTION_CROSSING_HLSLI
#define JUNCTION_CROSSING_HLSLI

// Shared "given a tet's 4 corners' labels+potentials and a ray through it,
// find the first present-label crossing and shade it" logic -- factored out
// of raymarchLatticePS.hlsl so raymarchEdgePS.hlsl's edge-data-driven walk
// (see the approved plan) can reuse the EXACT same reconstruction once it
// detects a non-homogeneous tet, instead of duplicating this block. One
// affine field PER DISTINCT LABEL present (corners sharing a label pool
// into one shared field rather than competing), plain own=+pot/foreign=-pot
// per corner, no beta/gamma -- see raymarchLatticePS.hlsl's own header
// comment for the full derivation history.
//
// Caller must already have included DistanceLattice.hlsli (for
// TetShapeGradients) and LabelPalette.hlsli (for LabelColorA) -- this header
// declares no register bindings of its own and doesn't re-include either,
// to avoid conflicting with whatever DISTANCE_GRID_CB_REGISTER the including
// file already chose.
//
// cornerDerivMult (edge-derivative multipliers, see the approved plan):
// per-corner scalar that scales that corner's contribution to each present
// label's gradient -- G_l[0] + dot(x-Pw[0], sum_c G_l[c]*m[c]*w_c) -- used
// consistently for BOTH the base value and the ray-direction slope (a single
// well-defined affine field per label, same gradient everywhere in the tet),
// NOT a base/slope split. That consistency matters: an earlier version used
// the unscaled gradient for the base and the scaled one only for the slope,
// which made the reconstructed tie surface implicitly depend on the ray
// origin (camera position) -- fine when every multiplier is 1.0 (both
// gradients are identical then, so it never showed up), but wrong once real,
// non-identity multipliers are populated (see
// buildAnalyticEdgeDerivMultCS.hlsl), since the whole point there is to
// reproduce a FIXED real surface (the analytic two-sphere junction circle)
// regardless of viewing angle. All-1.0 still reproduces the plain
// own=+pot/foreign=-pot affine formula exactly (raymarchLatticePS.hlsl,
// which has no edge data at all, always passes all-1.0). Callers are
// expected to have already collapsed each corner's (possibly several)
// hetero-edge multipliers down to one consistent value per the "pairwise
// equal" assumption; a corner touching no hetero edge at all should pass 1.0
// (identity -- multipliers are meaningless there).

struct JunctionHit {
    bool hit;
    float depthNdc; // clip-space z/w, only meaningful if hit
    float3 color;
};

// Pw/cornerLabel/cornerPot are all indexed 0..3, matching one tet's 4
// corners in a consistent (caller-defined) order. tCur/bestT bound the
// search to the portion of the ray currently inside this tet.
JunctionHit FindJunctionCrossing(
    float3 Pw[4], uint cornerLabel[4], float cornerPot[4], float cornerDerivMult[4],
    float3 ro, float3 rd, float4x4 viewProjTransform,
    float tCur, float bestT, float epsT, float epsSlope)
{
    JunctionHit result;
    result.hit = false;
    result.depthNdc = 1.0;
    result.color = float3(0, 0, 0);

    // Distinct labels present among this tet's 4 corners (up to 4, fewer
    // whenever corners share a label).
    uint present[4]; uint presentCount = 0;
    [unroll]
    for (uint cp = 0; cp < 4; cp++) {
        bool already = false;
        [unroll]
        for (uint pp = 0; pp < 4; pp++) {
            if (pp >= presentCount) break;
            if (present[pp] == cornerLabel[cp]) { already = true; break; }
        }
        if (!already) { present[presentCount] = cornerLabel[cp]; presentCount++; }
    }
    if (presentCount <= 1) return result;

    // Edge-derivative multipliers are only ever SOLVED for (and meant to
    // apply to) a genuine 3-label tet -- see buildAnalyticEdgeDerivMultCS.hlsl.
    // A 2-label tet sharing one of those same edges (very common -- an edge
    // is touched by many tets) must NOT inherit that unrelated multiplier;
    // its own plain reconstruction was already correct and has nothing to do
    // with whatever a neighboring 3-label tet solved. Force identity here
    // rather than trusting the caller to have zeroed it out.
    bool applyMult = (presentCount == 3);

    float3 w0, w1, w2, w3;
    TetShapeGradients(Pw[0], Pw[1], Pw[2], Pw[3], w0, w1, w2, w3);
    float3 wArr[4] = { w0, w1, w2, w3 };

    // One field per present label (not per corner): field l's value at
    // corner c is +itsOwnPotential where c's label matches l, -itsOwnPotential
    // everywhere else -- so corners sharing a label pool into one shared
    // field, each contributing its own real potential as a genuine sample.
    // Affine over the tet, so its value along the ray, G_l(t)=base+slope*t,
    // is a single line, computed once via the same wArr (tet geometry only).
    float lineBase[4], lineSlope[4]; // indexed by position in `present`, not corner index
    [unroll]
    for (uint pi = 0; pi < 4; pi++) {
        if (pi >= presentCount) break;
        uint l = present[pi];
        float G[4];
        [unroll]
        for (uint c5 = 0; c5 < 4; c5++) G[c5] = (cornerLabel[c5] == l) ? cornerPot[c5] : -cornerPot[c5];
        // Single multiplier-scaled gradient, used for BOTH base and slope --
        // see the header comment on why base and slope must share one
        // gradient (camera-independence). applyMult gates this to the
        // genuine 3-label case only (see above).
        float m0 = applyMult ? cornerDerivMult[0] : 1.0;
        float m1 = applyMult ? cornerDerivMult[1] : 1.0;
        float m2 = applyMult ? cornerDerivMult[2] : 1.0;
        float m3 = applyMult ? cornerDerivMult[3] : 1.0;
        float3 gradG = G[0] * m0 * wArr[0] + G[1] * m1 * wArr[1]
                     + G[2] * m2 * wArr[2] + G[3] * m3 * wArr[3];
        lineBase[pi] = G[0] + dot(ro - Pw[0], gradG);
        lineSlope[pi] = dot(rd, gradG);
    }

    // First t (nearest the camera) after tCur where two present labels' lines
    // tie AND are jointly the maximum among all present labels there -- the
    // classic "first breakpoint of an upper envelope of lines" search.
    float crossT = 1.0e30; uint crossA = 0, crossB = 0; bool foundCross = false;
    [unroll]
    for (uint pi2 = 0; pi2 < 4; pi2++) {
        if (pi2 >= presentCount) break;
        [unroll]
        for (uint pj = 0; pj < 4; pj++) {
            if (pj >= presentCount || pj <= pi2) continue;
            float slopeDiff = lineSlope[pj] - lineSlope[pi2];
            if (abs(slopeDiff) <= epsSlope) continue; // parallel (or coincident) -- never a fresh crossing
            float tc = (lineBase[pi2] - lineBase[pj]) / slopeDiff;
            if (tc <= tCur + epsT || tc > bestT + epsT || tc >= crossT) continue;
            float tieVal = lineBase[pi2] + lineSlope[pi2] * tc;
            bool valid = true;
            [unroll]
            for (uint pk = 0; pk < 4; pk++) {
                if (pk >= presentCount || pk == pi2 || pk == pj) continue;
                float vk = lineBase[pk] + lineSlope[pk] * tc;
                if (vk > tieVal + 1.0e-6) { valid = false; break; } // a 3rd label is already ahead here -- not the true crossing
            }
            if (valid) { crossT = tc; crossA = pi2; crossB = pj; foundCross = true; }
        }
    }
    if (!foundCross) return result;

    // Whichever of the pair has the SMALLER slope was dominant just before
    // the tie (the near/background side, receding); the other is what's
    // actually beyond the surface from the camera's side (approaching).
    uint winner = (lineSlope[crossA] < lineSlope[crossB]) ? crossA : crossB;
    uint crossWinner = (winner == crossA) ? crossB : crossA;
    uint winnerLabel = present[winner];
    uint crossLabel = present[crossWinner];

    float3 worldHit = ro + rd * crossT;
    float4 clipHit = mul(float4(worldHit, 1), viewProjTransform);

    // Interface normal: gradient of (G_crossLabel - G_winnerLabel) -- which
    // label is "positive" is arbitrary, only the SIGN used for orienting
    // toward the camera below matters for shading.
    float gDiff[4];
    [unroll]
    for (uint c6 = 0; c6 < 4; c6++) {
        float gW = (cornerLabel[c6] == winnerLabel) ? cornerPot[c6] : -cornerPot[c6];
        float gO = (cornerLabel[c6] == crossLabel) ? cornerPot[c6] : -cornerPot[c6];
        gDiff[c6] = gO - gW;
    }
    float3 gradN = gDiff[0] * wArr[0] + gDiff[1] * wArr[1] + gDiff[2] * wArr[2] + gDiff[3] * wArr[3];
    float3 n = (length(gradN) > 1.0e-8) ? normalize(gradN) : float3(0, 0, 1);
    float3 toCam = -rd;
    float3 nFacing = (dot(n, toCam) > 0.0) ? n : -n;

    float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
    float diff = saturate(dot(nFacing, lightDir)) * 0.7 + 0.3;

    result.hit = true;
    result.depthNdc = clipHit.z / clipHit.w;
    // crossLabel is the label that overtakes as t increases -- i.e. what's
    // actually beyond the surface from the camera's side (winnerLabel is the
    // near/background side that recedes), matching surfacePS.hlsl's "far
    // side" color convention.
    result.color = LabelColorA(crossLabel) * diff;
    return result;
}

#endif
