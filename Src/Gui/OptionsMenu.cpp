#include "OptionsMenu.hpp"
#include "Widgets/WidgetList.hpp"
#include "../Settings.hpp"

static constexpr float WIDGET_LIST_WIDTH = 400;
static constexpr float WIDGET_LIST_SPACING = 50;

static WidgetList leftWidgetList(WIDGET_LIST_WIDTH);
static WidgetList rightWidgetList(WIDGET_LIST_WIDTH);

bool optionsMenuOpen = false;

template <typename T>
std::vector<std::string> settingsCBOptions;

template <> std::vector<std::string> settingsCBOptions<eg::TextureQuality> { "Low", "Medium", "High" };
template <> std::vector<std::string> settingsCBOptions<QualityLevel> { "Very Low", "Low", "Medium", "High", "Very High" };
template <> std::vector<std::string> settingsCBOptions<DisplayMode> { "Windowed", "Fullscreen", "Fullscreen Windowed" };

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

Slider InitSettingsSlider(float Settings::*member, std::string label, float min, float max)
{
	Slider slider;
	slider.label = std::move(label);
	slider.min = min;
	slider.max = max;
	slider.getValue = [member] { return settings.*member; };
	slider.setValue = [member] (float value)
	{
		settings.*member = value;
		SettingsChanged();
	};
	return slider;
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
	
	leftWidgetList.AddWidget(SubtitleWidget("Display"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::displayMode, "Display Mode:"));
	leftWidgetList.AddWidget(std::move(resolutionCB));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::vsync, "VSync:", "On", "Off"));
	
	leftWidgetList.AddWidget(SubtitleWidget("Graphics"));
	textureQualityWidgetIndex = leftWidgetList.AddWidget(InitSettingsCB(&Settings::textureQuality, "Textures:"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::shadowQuality, "Shadows:"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::reflectionsQuality, "Reflections:"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::lightingQuality, "Lighting:"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::waterQuality, "Water:"));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::enableBloom, "Bloom:", "On", "Off"));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::enableFXAA, "FXAA:", "On", "Off"));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::gunFlash, "Gun Flash:", "Enabled", "Disabled"));
	leftWidgetList.AddWidget(InitSettingsSlider(&Settings::exposure, "Brightness:", 0.5f, 1.2f));
	Slider fovSlider = InitSettingsSlider(&Settings::fieldOfViewDeg, "Field of View:", 60, 100);
	fovSlider.displayValue = true;
	fovSlider.valueSuffix = u8"°";
	leftWidgetList.AddWidget(std::move(fovSlider));
	
	leftWidgetList.AddSpacing(20);
	leftWidgetList.AddWidget(Button("Back", []
	{
		SaveSettings();
		UpdateDisplayMode();
		optionsMenuOpen = false;
	}));
	
	
	rightWidgetList.AddWidget(SubtitleWidget("Input"));
	rightWidgetList.AddWidget(InitSettingsToggleButton(&Settings::lookInvertY, "Look Invert Y:", "Yes", "No"));
	rightWidgetList.AddWidget(InitSettingsSlider(&Settings::lookSensitivityMS, "Mouse Sensitivity:", 0.001f, 0.02f));
	
	ComboBox joystickAxesCB;
	joystickAxesCB.label = "Joystick Mapping:";
	joystickAxesCB.options = { "Left: Move, Right: Look", "Left: Look, Right: Move" };
	joystickAxesCB.getValue = [] { return (int)settings.flipJoysticks; };
	joystickAxesCB.setValue = [] (int value) { settings.flipJoysticks = value == 1; SettingsChanged(); };
	rightWidgetList.AddWidget(std::move(joystickAxesCB));
	rightWidgetList.AddWidget(InitSettingsSlider(&Settings::lookSensitivityGP, "Joystick Sensitivity:", 0.5f, 4.0f));
	
	rightWidgetList.AddWidget(SubtitleWidget("Key Bindings"));
	ComboBox keyBindingsConfigureModeCB;
	keyBindingsConfigureModeCB.label = "Bindings to Configure:";
	keyBindingsConfigureModeCB.options = { "Keyboard & Mouse", "Controller" };
	keyBindingsConfigureModeCB.getValue = [] { return (int)KeyBindingWidget::isConfiguringGamePad; };
	keyBindingsConfigureModeCB.setValue = [] (int value) { KeyBindingWidget::isConfiguringGamePad = value == 1; };
	rightWidgetList.AddWidget(std::move(keyBindingsConfigureModeCB));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Forward:", settings.keyMoveF));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Backward:", settings.keyMoveB));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Left:", settings.keyMoveL));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Right:", settings.keyMoveR));
	rightWidgetList.AddWidget(KeyBindingWidget("Jump:", settings.keyJump));
	rightWidgetList.AddWidget(KeyBindingWidget("Interact:", settings.keyInteract));
	rightWidgetList.AddWidget(KeyBindingWidget("Shoot:", settings.keyShoot));
	rightWidgetList.AddWidget(KeyBindingWidget("Menu:", settings.keyMenu));
	
	rightWidgetList.AddSpacing(std::max(leftWidgetList.Height() - WidgetList::WIDGET_SPACING - Button::height - rightWidgetList.Height(), 0.0f));
	rightWidgetList.AddWidget(Button("Reset All Settings", []
	{
		settings = {};
		SettingsChanged();
	}));
	
	leftWidgetList.relativeOffset = glm::vec2(-1, 0.5f);
	rightWidgetList.relativeOffset = glm::vec2(0, 0.5f);
	
	initialTextureQuality = settings.textureQuality;
}

EG_ON_INIT(InitOptionsMenu)

void UpdateOptionsMenu(float dt)
{
	KeyBindingWidget::anyKeyBindingPickingKey = false;
	
	constexpr float distFromCenter = WIDGET_LIST_SPACING / 2;
	leftWidgetList.position = glm::vec2(eg::CurrentResolutionX() - distFromCenter, eg::CurrentResolutionY()) / 2.0f;
	rightWidgetList.position = glm::vec2(eg::CurrentResolutionX() + distFromCenter, eg::CurrentResolutionY()) / 2.0f;
	
	leftWidgetList.Update(dt, true);
	rightWidgetList.Update(dt, true);
	
	ComboBox& textureQualityCB = std::get<ComboBox>(leftWidgetList.GetWidget(textureQualityWidgetIndex));
	const bool hasTextureWarning = !textureQualityCB.warning.empty();
	const bool shouldHaveTextureWarning = settings.textureQuality != initialTextureQuality;
	if (hasTextureWarning != shouldHaveTextureWarning)
	{
		textureQualityCB.warning = shouldHaveTextureWarning ? "Requires restart" : "";
	}
}

void DrawOptionsMenu(eg::SpriteBatch& spriteBatch)
{
	leftWidgetList.Draw(spriteBatch);
	rightWidgetList.Draw(spriteBatch);
}
