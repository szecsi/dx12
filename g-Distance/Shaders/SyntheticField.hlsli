#pragma once

// Shared by buildSyntheticBCS.hlsl (init) and smoothnessJacobiSyntheticCS.hlsl
// (runtime relabel) -- deliberately factored out so both call the exact same
// formula: a B-node's synthetic label must be picked the same way whether
// it's happening for the first time (init, non-unanimous own-cube case) or
// later because a sweep's 27-tap signed-kernel correction drove its
// potential negative.
//
// Given a B-node's 8 own A-corner (label, potential) pairs -- the SAME 8
// corners buildCandidatesCS.hlsl's tight own-cube scan uses:
// AIdx(i+(c&1),j+((c>>1)&1),k+((c>>2)&1)) for c=0..7 -- each corner's OWN
// label is a candidate; that candidate's score is the average, over all 8
// corners, of the corner's potential signed by whether ITS label agrees with
// the candidate (+1 agree, -1 disagree). Max score wins. Ties (duplicate
// labels among the 8 corners recomputing the identical score) just pick
// whichever tied corner is scanned first -- an equally valid winner either
// way, since the score only depends on the label value, not which corner
// proposed it.
void SyntheticVote8(uint labels[8], float potentials[8], out uint winnerLabel, out float winnerScore)
{
    winnerLabel = labels[0];
    winnerScore = -1.0e30;
    for (uint c = 0; c < 8; c++) {
        uint candidate = labels[c];
        float sum = 0.0;
        for (uint m = 0; m < 8; m++) {
            sum += potentials[m] * ((labels[m] == candidate) ? 1.0 : -1.0);
        }
        float score = sum * 0.125; // average over 8 corners
        if (score > winnerScore) { winnerScore = score; winnerLabel = candidate; }
    }
}
