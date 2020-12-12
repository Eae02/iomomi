#include "OptionsMenu.hpp"
#include "Widgets/WidgetList.hpp"
#include "../Settings.hpp"

static WidgetList optWidgetList(400);

bool optionsMenuOpen = false;

template <typename T>
std::vector<std::string> settingsCBOptions;

template <> std::vector<std::string> settingsCBOptions<eg::TextureQuality> { "Low", "Medium", "High" };
template <> std::vector<std::string> settingsCBOptions<QualityLevel> { "Very Low", "Low", "Medium", "High", "Very High" };
template <> std::vector<std::string> settingsCBOptions<DisplayMode> { "Windowed", "Fullscreen", "Fullscreen Windowed" };
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

ToggleButton InitSettingsToggleButton(bool Settings::*member, std::string label, std::string trueString, std::string falseString)
{
	ToggleButton button;
	button.label = std::move(label);
	button.trueString = std::move(trueString);
	button.falseString = std::move(falseString);
	button.getValue = [member] { return settings.*member; };
	button.setValue = [member] (bool value)
	{
		settings.*member = value;
		SettingsChanged();
	};
	return button;
}

static int fullscreenDisplayModeIndex;
static size_t textureQualityWidgetIndex;
static eg::TextureQuality initialTextureQuality;

void InitOptionsMenu()
{
	ComboBox resolutionCB;
	resolutionCB.label = "Fullscreen Resolution";
	fullscreenDisplayModeIndex = eg::NativeDisplayModeIndex();
	for (int i = 0; i < (int)eg::FullscreenDisplayModes().size(); i++)
	{
		eg::FullscreenDisplayMode mode = eg::FullscreenDisplayModes()[i];
		if (mode == settings.fullscreenDisplayMode)
		{
			fullscreenDisplayModeIndex = i;
		}
		resolutionCB.options.push_back(
			std::to_string(mode.resolutionX) + "x" + std::to_string(mode.resolutionY) + " @" + std::to_string(mode.refreshRate) + "Hz"
		);
	}
	resolutionCB.getValue = [] { return fullscreenDisplayModeIndex; };
	resolutionCB.setValue = [] (int value)
	{
		fullscreenDisplayModeIndex = value;
		settings.fullscreenDisplayMode = eg::FullscreenDisplayModes()[value];
	};
	
	optWidgetList.AddWidget(SubtitleWidget("Display"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::displayMode, "Display Mode:"));
	optWidgetList.AddWidget(std::move(resolutionCB));
	optWidgetList.AddWidget(InitSettingsToggleButton(&Settings::vsync, "VSync:", "On", "Off"));
	
	optWidgetList.AddWidget(SubtitleWidget("Graphics"));
	textureQualityWidgetIndex = optWidgetList.AddWidget(InitSettingsCB(&Settings::textureQuality, "Textures:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::shadowQuality, "Shadows:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::reflectionsQuality, "Reflections:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::lightingQuality, "Lighting:"));
	optWidgetList.AddWidget(InitSettingsCB(&Settings::waterQuality, "Water:"));
	optWidgetList.AddWidget(InitSettingsToggleButton(&Settings::enableBloom, "Bloom:", "On", "Off"));
	optWidgetList.AddWidget(InitSettingsToggleButton(&Settings::enableFXAA, "FXAA:", "On", "Off"));
	
	optWidgetList.AddWidget(SubtitleWidget("Input"));
	optWidgetList.AddWidget(InitSettingsToggleButton(&Settings::lookInvertY, "Look Invert Y:", "Yes", "No"));
	
	optWidgetList.AddWidget(Button("Back", []
	{
		SaveSettings();
		UpdateDisplayMode();
		optionsMenuOpen = false;
	}));
	
	optWidgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
	
	initialTextureQuality = settings.textureQuality;
}

EG_ON_INIT(InitOptionsMenu)

void UpdateOptionsMenu(float dt)
{
	optWidgetList.position = glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY()) / 2.0f;
	optWidgetList.Update(dt, true);
	
	ComboBox& textureQualityCB = std::get<ComboBox>(optWidgetList.GetWidget(textureQualityWidgetIndex));
	const bool hasTextureWarning = !textureQualityCB.warning.empty();
	const bool shouldHaveTextureWarning = settings.textureQuality != initialTextureQuality;
	if (hasTextureWarning != shouldHaveTextureWarning)
	{
		textureQualityCB.warning = shouldHaveTextureWarning ? "Requires restart" : "";
	}
}

void DrawOptionsMenu(eg::SpriteBatch& spriteBatch)
{
	optWidgetList.Draw(spriteBatch);
}
