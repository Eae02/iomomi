#include "OptionsMenu.hpp"
#include "Widgets/WidgetList.hpp"
#include "../Settings.hpp"

static WidgetList optWidgetList(300);

bool optionsMenuOpen = false;

template <typename T>
std::vector<std::string> settingsCBOptions;

template <> std::vector<std::string> settingsCBOptions<eg::TextureQuality> { "Low", "Medium", "High" };
template <> std::vector<std::string> settingsCBOptions<QualityLevel> { "Very Low", "Low", "Medium", "High", "Very High" };
template <> std::vector<std::string> settingsCBOptions<bool> { "No", "Yes" };

template <typename T>
ComboBox InitSettingsCB(T Settings::*member, std::string label)
{
	ComboBox cb;
	cb.label = std::move(label);
	cb.options = settingsCBOptions<T>;
	cb.getValue = [member] { return (int)(settings.*member); };
	cb.setValue = [member] (int value)
	{
		settings.*member = (T)value;
		SettingsChanged();
	};
	return cb;
}

void InitOptionsMenu()
{
	optWidgetList.AddWidget(SubtitleWidget("Graphics Quality"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::textureQuality, "Textures:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::shadowQuality, "Shadows:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::reflectionsQuality, "Reflections:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::lightingQuality, "Lighting:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::waterQuality, "Water:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::enableBloom, "Bloom:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::enableFXAA, "FXAA:"));
	
	optWidgetList.AddWidget(SubtitleWidget("Input"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::lookInvertY, "Look Invert Y:"));
	
	optWidgetList.AddWidget(Button("Back", []
	{
		SaveSettings();
		optionsMenuOpen = false;
	}));
	
	optWidgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
}

EG_ON_INIT(InitOptionsMenu)

void UpdateOptionsMenu(float dt)
{
	optWidgetList.position = glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY()) / 2.0f;
	optWidgetList.Update(dt, true);
}

void DrawOptionsMenu(eg::SpriteBatch& spriteBatch)
{
	optWidgetList.Draw(spriteBatch);
}
