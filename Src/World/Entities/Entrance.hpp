#pragma once

#include "ECEditorVisible.hpp"
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
	
	static void InitPlayer(eg::Entity& entity, class Player& player);
	
	static void Update(const WorldUpdateArgs& updateArgs);
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static Door GetDoorDescription(const eg::Entity& entity);
	
	static eg::EntitySignature EntitySignature;
	
	static eg::IEntitySerializer* EntitySerializer;
	
private:
	static void CalcClipping(eg::Entity& entity, ClippingArgs& args);
	
	static void Draw(eg::Entity& entity, eg::MeshBatch& meshBatch);
	
	static void EditorDraw(eg::Entity& entity, bool selected, const EditorDrawArgs& drawArgs);
	static void EditorRenderSettings(eg::Entity& entity);
	
	Type m_type = Type::Entrance;
	float m_doorOpenProgress = 0;
	float m_timeBeforeClose = 0;
};