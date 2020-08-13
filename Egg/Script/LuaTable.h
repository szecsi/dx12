#pragma once
#include <sstream>
extern "C"
{
#include "lua.h"
#include "lualib.h"
}
#include "luabind/luabind.hpp"
#include "luabind/adopt_policy.hpp"


#include "EnumReflectionMap.h"
#include "Egg/Math/math.h"
//#include "App/ThrowOnFail.h"

namespace Egg { namespace Script
{

	/// Helper class to access fields of lua tables provided as luabind::object.
	class LuaTable
	{
	public:
		/// The lua table.
		luabind::object luaObject;

		std::string errorLocation;

		void throwError(const std::string& errorMessage)
		{
			lua_State* luaState = luaObject.interpreter();
			lua_Debug ar;
			lua_getstack(luaState, 1, &ar);
			lua_getinfo(luaState, "nSl", &ar);
			int line = ar.currentline;
			const char* source = ar.source;
			throw std::exception{ (errorMessage + "(" + source + ":" + std::to_string(line) + ")").c_str() };
			//HrException(E_FAIL, errorMessage.c_str(), source, line);
		}

		/// Constructor.
		/// @param luaObject the lua object, should be a lua table, or the program exits with an error message
		LuaTable(luabind::object luaObject, std::string errorLocation)
			:errorLocation(errorLocation)
		{
			if(luabind::type(luaObject) != LUA_TTABLE)
			{
				throwError(errorLocation + ": Lua table expected.");
			}
			this->luaObject = luaObject;
		}

		luabind::object getLuaBindObject(const char* paramname)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			return a;
		}

		template<typename T>
		std::shared_ptr<T> get(const std::string& paramname, std::shared_ptr<T> defaultValue)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if (luabind::type(a) == LUA_TNIL)
				return defaultValue;
			return luabind::object_cast<std::shared_ptr<T>>(a);
		}

		template<typename T>
		std::shared_ptr<T> get(const std::string& paramname)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if (luabind::type(a) == LUA_TNIL)
				throwError(errorLocation + ": Required lua table entry '" + paramname + "' not found or has nil value.");
			try {
				return luabind::object_cast<std::shared_ptr<T>>(a);
			}
			catch (luabind::cast_failed castFailed)
			{
				throwError("Specified " + paramname + " is not proper type " + typeid(T).name());
			}
			return nullptr;
		}

		/// Retrieves a table field as a string.
		/// @param paramname the name of the field
		/// @param defaultValue the string that should be returned if the specified field does not exit, or NULL to indicate a required field
		/// @return the value of the table field
		std::string getString(const char* paramname, const char* defaultValue = NULL)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
			{
				if(defaultValue == NULL)
				{
					throwError(errorLocation + ": Required parameter '" + paramname + "' not found.");
				}
				return defaultValue;
			}
			return luabind::object_cast<std::string>(a);
		}

		/// Retrieves a table field as an integer.
		/// @param paramname the name of the field
		/// @param defaultValue the value that should be returned if the specified field does not exit
		/// @return the value of the table field
		int getInt(const char* paramname, int defaultValue = 0)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
			int ret = defaultValue;
			try
			{
				ret = luabind::object_cast<int>(a);
			}
			catch(...)
			{
				std::stringstream ss;
				ss << errorLocation << ": Conversion of '" << paramname << "' value '" << a << "' to int failed.";
				throwError(ss.str());
			}
			return ret;
		}

		/// Retrieves a table field as a bool.
		/// @param paramname the name of the field
		/// @param defaultValue the value that should be returned if the specified field does not exit
		/// @return the value of the table field
		bool getBool(const char* paramname, bool defaultValue = false)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if (luabind::type(a) == LUA_TNIL)
				return defaultValue;
			bool ret = defaultValue;
			try
			{
				ret = luabind::object_cast<bool>(a);
			}
			catch (...)
			{
				std::stringstream ss;
				ss << errorLocation << ": Conversion of '" << paramname << "' value '" << a << "' to bool failed.";
				throwError(ss.str());
			}
			return ret;
		}

		/// Retrieves a table field as a float.
		/// @param paramname the name of the field
		/// @param defaultValue the value that should be returned if the specified field does not exit
		/// @return the value of the table field
		float getFloat(const char* paramname, float defaultValue = 0)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
			float ret = defaultValue;
			try
			{
				ret = luabind::object_cast<float>(a);
			}
			catch(...)
			{
				std::stringstream ss;
				ss << errorLocation << ": Conversion of '" << paramname << "' value '" << a << "' to float failed.";
				throwError(ss.str());
			}
			return ret;
		}

		/// Retrieves a table field (which must be a table itself) as a Float3.
		/// @param paramname the name of the field
		/// @param defaultValue the value that should be returned if the specified field does not exist
		/// @return the value of the table field
		Egg::Math::Float3 getFloat3(const char* paramname, Egg::Math::Float3 defaultValue = Egg::Math::Float3(1, 1, 1))
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
			if( luabind::type(a) != LUA_TTABLE)
			{
				std::stringstream ss;
				ss << errorLocation << ": Table expected for Float3 parameter '" << paramname << "'.";
				throwError(ss.str());
			}
			LuaTable vectorElements(a, errorLocation);
			return Egg::Math::Float3 (
				vectorElements.getFloat("x"),
				vectorElements.getFloat("y"),
				vectorElements.getFloat("z") );
		}

		/// Evaluates the table as a Float3.
		/// @param defaultValue the values that should be returned where the channel fields do not exist
		/// @return the value of the vactor
		Egg::Math::Float3 asFloat3(Egg::Math::Float3 defaultValue = Egg::Math::Float3(1, 1, 1))
		{
			return Egg::Math::Float3 (
				getFloat("x", defaultValue.x),
				getFloat("y", defaultValue.y),
				getFloat("z", defaultValue.z) );
		}

		Egg::Math::Float4 getFloat4(const char* paramname, Egg::Math::Float4 defaultValue = Egg::Math::Float4(1, 1, 1, 1))
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
			if( luabind::type(a) != LUA_TTABLE)
			{
				std::stringstream ss;
				ss << errorLocation << ": Table expected for Float4 parameter '" << paramname << "'.";
				throwError(ss.str());
			}
			LuaTable vectorElements(a, errorLocation);
			return Egg::Math::Float4 (
				vectorElements.getFloat("x"),
				vectorElements.getFloat("y"),
				vectorElements.getFloat("z"),
				vectorElements.getFloat("w"));
		}

		/// Evaluates the table as a Float4.
		/// @param defaultValue the values that should be returned where the channel fields do not exist
		/// @return the value of the Float4
		Egg::Math::Float4 asFloat4(Egg::Math::Float4 defaultValue = Egg::Math::Float4(1, 1, 1, 1))
		{
			return Egg::Math::Float4 (
				getFloat("x", defaultValue.x),
				getFloat("y", defaultValue.y),
				getFloat("z", defaultValue.z),
				getFloat("w", defaultValue.w));
		}

		int getKeycode(const char* paramname, int defaultValue = VK_F4)
		{
			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
		
			std::string kks = luabind::object_cast<std::string>(a);
			if( kks == "VK_SPACE" ) return VK_SPACE;
			if( kks == "VK_NUMPAD0" ) return VK_NUMPAD0;
			if( kks == "VK_NUMPAD1" ) return VK_NUMPAD1;
			if( kks == "VK_NUMPAD2" ) return VK_NUMPAD2;
			if( kks == "VK_NUMPAD3" ) return VK_NUMPAD3;
			if( kks == "VK_NUMPAD4" ) return VK_NUMPAD4;
			if( kks == "VK_NUMPAD5" ) return VK_NUMPAD5;
			if( kks == "VK_NUMPAD6" ) return VK_NUMPAD6;
			if( kks == "VK_NUMPAD7" ) return VK_NUMPAD7;
			if( kks == "VK_NUMPAD8" ) return VK_NUMPAD8;
			if( kks == "VK_NUMPAD9" ) return VK_NUMPAD9;

			return kks.at(0);
		}

		template <typename E>
		E getEnum(const char* paramname, const E& defaultValue)
		{
			const EnumReflectionMap<E>& enumReflectionMap = EnumReflectionMap<E>::getMap();

			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
			std::string fieldname = luabind::object_cast<std::string>(a);

			typename EnumReflectionMap<E>::const_iterator i = enumReflectionMap.begin();
			typename EnumReflectionMap<E>::const_iterator e = enumReflectionMap.end();
			while(i != e)
			{
				if(i->first == fieldname)
					return i->second;
				i++;
			}

			return defaultValue;
		}

		template <typename E, typename ECombo>
		ECombo getEnumCombination(const char* paramname, ECombo defaultValue = ECombo(0))
		{
			const EnumReflectionMap<E>& enumReflectionMap = EnumReflectionMap<E>::getMap();

			luabind::adl::index_proxy<luabind::object> a = luaObject[paramname];
			if( luabind::type(a) == LUA_TNIL)
				return defaultValue;
			ECombo combo(0);
			if( luabind::type(a) != LUA_TTABLE)
			{
				std::stringstream ss;
				ss << errorLocation << ": Table expected for enum combination parameter '" << paramname << "'.";
				throwError(ss.str());
			}
			LuaTable aTable(a, errorLocation);
			for (luabind::iterator i(a), end; i != end; ++i)
			{
				std::string vname = luabind::object_cast<std::string>(*i);

				typename EnumReflectionMap<E>::const_iterator ei = enumReflectionMap.begin();
				typename EnumReflectionMap<E>::const_iterator ee = enumReflectionMap.end();
				while(ei != ee)
				{
					if(ei->first == vname)
						combo |= ei->second;
					ei++;
				}
			}
			return combo;
		}
	};

}}