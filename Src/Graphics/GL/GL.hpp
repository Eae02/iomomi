#pragma once

namespace gl
{
	void Init();
	
	extern bool HasVersion45;
	
	template <GLenum E>
	inline void SetEnabled(bool enable)
	{
		static bool currentlyEnabled = false;
		
		if (enable && !currentlyEnabled)
			glEnable(E);
		else if (!enable && currentlyEnabled)
			glDisable(E);
		currentlyEnabled = enable;
	}
}
