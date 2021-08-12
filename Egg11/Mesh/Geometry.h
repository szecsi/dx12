#pragma once

namespace Egg11 { namespace Mesh
{
	GG_CLASS(Geometry)
	public:

		virtual ~Geometry(void)
		{
		}

		virtual void getElements(const D3D11_INPUT_ELEMENT_DESC*& elements, unsigned int& nElements)=0;

		virtual void draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)=0;
		virtual Microsoft::WRL::ComPtr<ID3D11Buffer> getPrimaryBuffer()=0;
	GG_ENDCLASS

}} // namespace Egg11::Mesh