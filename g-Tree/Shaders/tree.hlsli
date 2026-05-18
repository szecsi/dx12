#pragma once

static const uint nMinPiecesPerTree = 33;
static const uint nMaxPiecesPerTree = 127;
static const uint nAveragePieces    = (nMinPiecesPerTree + nMaxPiecesPerTree) / 2;
static const uint nTreeModels       = 32 * 27;

#define RootSigTree \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "CBV(b0)," \
    "CBV(b1)," \
    "CBV(b2)," \
    "DescriptorTable(SRV(t0, numDescriptors=3))"