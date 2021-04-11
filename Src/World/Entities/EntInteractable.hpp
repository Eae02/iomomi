#pragma once

#include "../World.hpp"

struct InteractControlHint
{
	std::string_view message;
	std::optional<OptionalControlHintType> optControlHintType;
	const struct KeyBinding* keyBinding;
};

class EntInteractable
{
public:
	//The interact function will be called when the player interacts with the entity
	virtual void Interact(class Player& player) = 0;
	
	//The check interaction function will be called for every interactable entity to see if it can be interacted with.
	// If interaction is possible, a positive integer should be returned, otherwise return 0. If there are multiple
	// interactable entities, the one with the greatest return value from this function will be selected.
	virtual int CheckInteraction(const class Player& player, const class PhysicsEngine& physicsEngine) const = 0;
	
	virtual std::optional<InteractControlHint> GetInteractControlHint() const = 0;
};
