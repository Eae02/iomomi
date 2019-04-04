#pragma once

enum class EntityEditorDrawMode
{
	Default,
	Selected,
	Targeted
};

struct EditorDrawMessage : eg::Message<EditorDrawMessage>
{
	eg::SpriteBatch* spriteBatch;
	eg::MeshBatch* meshBatch;
	class PrimitiveRenderer* primitiveRenderer;
	std::function<EntityEditorDrawMode(const eg::Entity*)> getDrawMode;
};

struct EditorRenderImGuiMessage : eg::Message<EditorRenderImGuiMessage> { };

struct ECEditorVisible
{
	const char* displayName;
	int iconIndex;
	
	static void RenderDefaultSettings(eg::Entity& entity);
	
	ECEditorVisible()
		: ECEditorVisible("Unnamed Entity") { }
	
	ECEditorVisible(const char* _displayName, int _iconIndex = 5)
		: displayName(_displayName), iconIndex(_iconIndex) { }
};
