#pragma once
#include "Mesh/Shaded.h"
#include "Mesh/Mien.h"

namespace Egg11 { namespace Mesh
{
	GG_CLASS(Flip)
		typedef std::map<Mien, Shaded::P> MienShadedMap;
		MienShadedMap mienShadedMap;

	protected:
		Flip();
	public:

		~Flip(void);

		void add(Mien mien, Shaded::P shaded);

		Shaded::P& getShaded(Mien mien)
		{
			return mienShadedMap[mien];
		}

		void draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Mien mien);
	GG_ENDCLASS

}} // namespace Egg11::Mesh
