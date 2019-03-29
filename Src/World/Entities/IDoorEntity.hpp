#pragma once

struct Door
{
	glm::vec3 position;
	glm::vec3 normal;
	float radius;
};

class IDoorEntity
{
public:
	virtual Door GetDoorDescription() const = 0;
};
