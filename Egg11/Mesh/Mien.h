#pragma once

namespace Egg11 { namespace Mesh {

	class Mien
	{
		unsigned int id;
		static unsigned int nextId;
		
	public:

		Mien();
		bool operator<(const Mien& co) const;
		bool operator==(const Mien& co) const;

		static const Mien invalid;

		inline bool isValid() const {return id != 0;}

		struct Compare
		{
			bool operator() (const Mien& a, const Mien& b) const
			{
				return a < b;
			}
		};
	};

}} // namespace Egg11::Mesh