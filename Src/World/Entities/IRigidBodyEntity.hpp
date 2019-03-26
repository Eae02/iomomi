#pragma once

#include "../BulletPhysics.hpp"

class IRigidBodyEntity
{
public:
	virtual btRigidBody* GetRigidBody() = 0;
};
