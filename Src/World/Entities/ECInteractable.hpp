#pragma once

struct ECInteractable
{
	std::string_view interactDescription;
	
	//The interact callback will be invoked when the player interacts with the entity
	using InteractCallback = void(*)(eg::Entity& entity, class Player& player);
	InteractCallback interact;
	
	//The check interaction callback will be invoked for every interactable entity to see if it can be interacted with.
	// If interaction is possible, a positive integer should be returned, otherwise return 0.
	// If there are multiple interactable entities, the one with the greatest return value will be selected.
	using CheckInteractionCallback = int(*)(const eg::Entity& entity, const class Player& player);
	CheckInteractionCallback checkInteraction;
	
	ECInteractable()
		: interact(nullptr), checkInteraction(nullptr) { }
	
	ECInteractable(std::string_view _description, InteractCallback _interact, CheckInteractionCallback _checkInteraction)
		: interactDescription(_description), interact(_interact), checkInteraction(_checkInteraction) { }
};
