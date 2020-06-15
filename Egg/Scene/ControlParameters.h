#pragma once

namespace Egg { namespace Scene
{
	GG_DECL(Entity);

	/// Structure that holds control information, including keyboard and mouse input information
	class ControlParameters
	{
	public:
		/// timestep
		float dt;
		/// Array of key pressed state variables. Addressed by virtual key codes, true if key is pressed.
		bool keyPressed[0xff];

		/// Updates input state by processing message.
		virtual void processMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			if(uMsg == WM_KEYDOWN)
				keyPressed[wParam] = true;
			else if(uMsg == WM_KEYUP)
				keyPressed[wParam] = false;
			else if(uMsg == WM_KILLFOCUS)
			{
				for(unsigned int i=0; i<0xff; i++)
					keyPressed[i] = false;
			}
		}

		std::vector<EntityP> spawn;

		/// Constructor. Intializes input state.
		ControlParameters()
		{
			for(unsigned int i=0; i<0xff; i++)
				keyPressed[i] = false;
		}
	};
}}