#pragma once
#include "../Common.h"
#include "Egg/Scene/ControlState.h"
extern "C"
{
#include "lua.h"
#include "lualib.h"
}
#include "luabind/luabind.hpp"
#include "luabind/adopt_policy.hpp"

namespace Egg { namespace Control {
	/// Application class with scene management
	GG_SUBCLASS(LuaControlState, Egg::Scene::ControlState)
protected:
			luabind::object controlState;
		LuaControlState(Egg::Scene::Entity::P entity, luabind::object controlState) :
			Egg::Scene::ControlState(entity),
			controlState(controlState) {}
public:
		bool control(float dt)
		{
			luabind::set_pcall_callback(nullptr);
			try{
			if (luabind::type(controlState) == LUA_TTABLE)
			{
				if (luabind::type(controlState["script"]) == LUA_TFUNCTION)
					controlState["script"](entity, controlState);
				if (luabind::type(controlState["killed"]) != LUA_TNIL)
					return false;
			}
			}
			catch (std::exception exception) { 
				luabind::object msg(luabind::from_stack(controlState.interpreter(), 0));
				std::ostringstream str;
				str << "lua> run-time error in control script: " << msg;

				throw std::exception(str.str().c_str());
			}

			return true;
		}

		void onContact(Egg::Scene::Entity::P other)
		{
			luabind::set_pcall_callback(nullptr);
			try {
				if (luabind::type(controlState) == LUA_TTABLE)
					if (luabind::type(controlState["onContact"]) == LUA_TFUNCTION)
						controlState["onContact"](entity, controlState
							//other->GetControlState()
							//luabind::object()
							);
			}
			catch (std::exception exception) {
				luabind::object msg(luabind::from_stack(controlState.interpreter(), 0));
				std::ostringstream str;
				str << "lua> run-time error in contact script: " << msg;

				throw std::exception(str.str().c_str());
			}
		}



	GG_ENDCLASS
}}