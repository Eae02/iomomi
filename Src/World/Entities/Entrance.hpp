#pragma once

#include "ECEditorVisible.hpp"
#include "Messages.hpp"
#include "../Clipping.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../Door.hpp"

class ECEntrance
{
public:
	enum class Type
	{
		Entrance = 0,
		Exit = 1
	};
	
	Type GetType() const
	{
		return m_type;
	}
	
	void SetType(Type type)
	{
		m_type = type;
	}
	
	const std::string& ExitName() const
	{
		return m_name;
	}
	
	void HandleMessage(eg::Entity& entity, const CalculateCollisionMessage& message);
	void HandleMessage(eg::Entity& entity, const DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
	
	static void InitPlayer(eg::Entity& entity, class Player& player);
	
	static void MovePlayer(const eg::Entity& oldExit, const eg::Entity& newEntrance, class Player& player);
	
	static void Update(const WorldUpdateArgs& updateArgs, eg::Entity** switchEntranceOut);
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static Door GetDoorDescription(const eg::Entity& entity);
	
	static eg::EntitySignature EntitySignature;
	
	static eg::IEntitySerializer* EntitySerializer;
	
	static eg::MessageReceiver MessageReceiver;
	
private:
	std::string m_name = "main";
	Type m_type = Type::Entrance;
	float m_doorOpenProgress = 0;
	float m_timeBeforeClose = 0;
};
