#pragma once
#include <map>
#include <string>

namespace Egg { namespace Script
{
	template <typename E>
	class EnumReflectionMap : private std::map<std::string, E>
	{
	public:
		using std::map<std::string, E>::begin;
		using std::map<std::string, E>::end;
		using std::map<std::string, E>::operator[];
		using std::map<std::string, E>::iterator;
		using std::map<std::string, E>::const_iterator;
		using std::map<std::string, E>::find;
		using std::map<std::string, E>::size;
		EnumReflectionMap<E>& add(const std::string& key, const E& value)
		{
			(*this)[key] = value;
			return *this;
		}

		static EnumReflectionMap<E>& getMap()
		{
			static EnumReflectionMap<E> singleton;
			return singleton;
		}
	};

}}