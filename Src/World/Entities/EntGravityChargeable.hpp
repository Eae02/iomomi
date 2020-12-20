#pragma once

#include "../Dir.hpp"

class EntGravityChargeable
{
public:
	virtual bool SetGravity(Dir newGravity) = 0;
	virtual bool ShouldShowGravityBeamControlHint(Dir newGravity) { return false; }
};
