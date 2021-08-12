// not using precompiled headers
// this is a workaround for missing guid def for shader reflection when d3dx11 in not linked
// when this starts causing duplicate symbols, it must be removed
#define INITGUID
#include <D3D11Shader.h>