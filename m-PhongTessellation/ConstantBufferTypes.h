#pragma once

#include <Egg/Math/float4x4.h>

using namespace Egg::Math;

__declspec(align(16)) struct PerObjectCb {
    float4x4 modelMat;
    float4x4 modelMatInverse;
};

__declspec(align(16)) struct PerFrameCb {
    float4x4 viewProjMat;
    float4x4 rayDirMat;
    float4 cameraPos;
    float4 lightPos;
    float4 lightPowerDensity;
    float4 billboardSize;
};
