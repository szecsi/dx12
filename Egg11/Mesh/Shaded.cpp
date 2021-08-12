#include "stdafx.h"
#include "Mesh/Shaded.h"

using namespace Egg11;

void Mesh::Shaded::draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	material->apply(context);
	context->IASetInputLayout(inputLayout.Get());
	geometry->draw(context);
}