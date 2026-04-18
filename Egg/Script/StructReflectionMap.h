#pragma once
#include <map>
#include <vector>
#include <string>

namespace Egg { namespace Script {

struct StructMember {
	std::string name;
	size_t offset;
	size_t size;
};

class StructMemberList : public std::vector<StructMember> {
public:
	StructMemberList& add(const std::string& name, size_t offset, size_t size) {
		push_back({ name, offset, size });
		return *this;
	}
};

class StructReflectionMap : private std::map<std::string, StructMemberList> {
public:
	using std::map<std::string, StructMemberList>::begin;
	using std::map<std::string, StructMemberList>::end;
	using std::map<std::string, StructMemberList>::find;

	StructMemberList& get(const std::string& typeName) {
		return (*this)[typeName];
	}

	static StructReflectionMap& getMap() {
		static StructReflectionMap singleton;
		return singleton;
	}
};

}} // namespace Egg::Script

#define GG_STRUCT(T) { using _GG_T = T; Egg::Script::StructReflectionMap::getMap().get(#T)
#define GG_MEMBER(x) .add(#x, offsetof(_GG_T, x), sizeof(_GG_T::x))
#define GG_ENDSTRUCT ; }
