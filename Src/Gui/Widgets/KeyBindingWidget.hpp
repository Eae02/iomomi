#pragma once

#include "../../Settings.hpp"
#include "OptionWidgetBase.hpp"

class KeyBindingWidget : public OptionWidgetBase
{
public:
	KeyBindingWidget(std::string label, KeyBinding& binding);

	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;

	static bool isConfiguringGamePad;
	static bool anyKeyBindingPickingKey;

private:
	KeyBinding* m_binding;
	bool m_isPickingKey = false;
	float m_highlightIntensity = 0;
	float m_cancelButtonAlpha = 0;
	float m_cancelHighlightIntensity = 0;
	eg::Rectangle m_cancelRectangle;
};
