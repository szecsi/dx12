#pragma once
#include "../Common.h"
#include "ControlParameters.h"

namespace Egg { namespace Scene
{
	GG_DECL(Entity)

	GG_CLASS(ControlState)
	protected:
		EntityP entity;
		ControlState(EntityP entity) :entity(entity) {}
	public:
		virtual bool control(float dt) = 0;
		virtual void onContact(EntityP other) = 0;

	GG_ENDCLASS

}}
