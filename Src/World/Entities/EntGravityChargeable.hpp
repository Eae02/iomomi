#pragma once

#include "../Dir.hpp"

class EntGravityChargeable
{
public:
	virtual bool SetGravity(Dir newGravity) = 0;
};
