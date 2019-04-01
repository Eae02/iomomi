#pragma once

struct EditorDrawArgs
{
	eg::SpriteBatch* spriteBatch;
	eg::MeshBatch* meshBatch;
	class PrimitiveRenderer* primitiveRenderer;
};

struct ECEditorVisible
{
	using DrawCallback = void(*)(eg::Entity& entity, bool selected, const EditorDrawArgs& drawArgs);
	using RenderSettingsCallback = void(*)(eg::Entity& entity);
	
	const char* displayName;
	int iconIndex;
	DrawCallback editorDraw;
	RenderSettingsCallback editorRenderSettings;
	
	static void RenderDefaultSettings(eg::Entity& entity);
	
	ECEditorVisible()
		: ECEditorVisible(nullptr, nullptr) { }
	
	ECEditorVisible(const char* _displayName, DrawCallback _editorDraw)
		: ECEditorVisible(_displayName, 5, _editorDraw, &ECEditorVisible::RenderDefaultSettings) { }
	
	ECEditorVisible(const char* _displayName, DrawCallback _editorDraw, RenderSettingsCallback _editorRenderSettings)
		: ECEditorVisible(_displayName, 5, _editorDraw, _editorRenderSettings) { }
	
	ECEditorVisible(const char* _displayName, int _iconIndex, DrawCallback _editorDraw, RenderSettingsCallback _editorRenderSettings)
		: displayName(_displayName), iconIndex(_iconIndex), editorDraw(_editorDraw), editorRenderSettings(_editorRenderSettings) { }
};
