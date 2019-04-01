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
	
	DrawCallback editorDraw = nullptr;
	RenderSettingsCallback editorRenderSettings = nullptr;
	const char* displayName;
	int iconIndex = 5;
	
	static void RenderDefaultSettings(eg::Entity& entity);
	
	void Init(const char* _displayName, DrawCallback draw, RenderSettingsCallback renderSettings)
	{
		editorDraw = draw;
		editorRenderSettings = renderSettings;
		displayName = _displayName;
	}
	
	void Init(const char* _displayName, int _iconIndex,
		DrawCallback draw, RenderSettingsCallback renderSettings)
	{
		editorDraw = draw;
		editorRenderSettings = renderSettings;
		displayName = _displayName;
		iconIndex = _iconIndex;
	}
};
