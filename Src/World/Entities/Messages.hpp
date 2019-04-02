#pragma once

#include <functional>

struct DrawMessage : eg::Message<DrawMessage>
{
	eg::MeshBatch* meshBatch;
};

struct CalculateCollisionMessage : eg::Message<CalculateCollisionMessage>
{
	struct ClippingArgs* clippingArgs;
};

struct EditorDrawMessage : eg::Message<EditorDrawMessage>
{
	eg::SpriteBatch* spriteBatch;
	eg::MeshBatch* meshBatch;
	class PrimitiveRenderer* primitiveRenderer;
	std::function<void(const eg::Entity&)> isSelected;
};

struct EditorRenderSettingsMessage : eg::Message<EditorRenderSettingsMessage> { };
