#include "OptionsMenu.hpp"
#include "Widgets/WidgetList.hpp"
#include "Scroll.hpp"
#include "../Settings.hpp"

static constexpr float WIDGET_LIST_WIDTH = 420;
static constexpr float WIDGET_LIST_SPACING = 50;

static WidgetList leftWidgetList(WIDGET_LIST_WIDTH, 8);
static WidgetList rightWidgetList(WIDGET_LIST_WIDTH, 8);

static ScrollPanel optionsScrollPanel;

static Button backButton;

bool optionsMenuOpen = false;

template <typename T>
std::vector<std::string> settingsCBOptions;

template <> std::vector<std::string> settingsCBOptions<ViewBobbingLevel> { "Off", "Low", "Normal" };
template <> std::vector<std::string> settingsCBOptions<eg::TextureQuality> { "Low", "Medium", "High" };
template <> std::vector<std::string> settingsCBOptions<SSAOQuality> { "Off", "Low", "Medium", "High" };
template <> std::vector<std::string> settingsCBOptions<QualityLevel> { "Very Low", "Low", "Medium", "High", "Very High" };
template <> std::vector<std::string> settingsCBOptions<DisplayMode> { "Windowed", "Fullscreen", "Fullscreen Windowed" };

template <typename T>
ComboBox InitSettingsCB(T Settings::*member, std::string label, bool reverseOptions = false)
{
	ComboBox cb;
	cb.label = std::move(label);
	cb.options = settingsCBOptions<T>;
	if (reverseOptions)
		std::reverse(cb.options.begin(), cb.options.end());
	cb.getValue = [=]
	{
		int v = (int)(settings.*member);
		if (reverseOptions)
			v = (int)settingsCBOptions<T>.size() - 1 - v;
		return v;
	};
	cb.setValue = [=] (int value)
	{
		if (reverseOptions)
			value = (int)settingsCBOptions<T>.size() - 1 - value;
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

std::string_view VolumeAndBrightnessGetDisplayValueString(float value)
{
	if (value <= 0)
		return "Muted";
	int val = std::max((int)std::round(value * 100), 1);
	static char buffer[16];
	snprintf(buffer, sizeof(buffer), "%d%%", val);
	return buffer;
}

Slider InitVolumeSlider(float Settings::*member, std::string label)
{
	Slider slider = InitSettingsSlider(member, label, 0, 1.5f);
	slider.getDisplayValueString = VolumeAndBrightnessGetDisplayValueString;
	return slider;
}

ToggleButton InitSettingsToggleButton(bool Settings::*member, std::string label,
                                      std::string trueString, std::string falseString)
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

std::string FilterGPUNameString(std::string_view inputString)
{
	std::string outputString;
	int parenDepth = 0;
	for (char c : inputString)
	{
		if (c == '(')
			parenDepth++;
		if (parenDepth == 0)
			outputString.push_back(c);
		if (c == ')')
			parenDepth--;
	}
	return outputString;
}

void InitOptionsMenu()
{
#ifndef __EMSCRIPTEN__
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
		std::ostringstream labelStream;
		labelStream << mode.resolutionX << "x" << mode.resolutionY << " \e@" << mode.refreshRate << "Hz\e";
		resolutionCB.options.push_back(labelStream.str());
	}
	resolutionCB.getValue = [] { return fullscreenDisplayModeIndex; };
	resolutionCB.setValue = [] (int value)
	{
		fullscreenDisplayModeIndex = value;
		settings.fullscreenDisplayMode = eg::FullscreenDisplayModes()[value];
	};
	
	leftWidgetList.AddWidget(SubtitleWidget("Display"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::displayMode, "Display Mode"));
	leftWidgetList.AddWidget(std::move(resolutionCB));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::vsync, "VSync", "On", "Off"));
	
	ComboBox apiComboBox;
	apiComboBox.label = "Graphics API";
	apiComboBox.restartRequiredIfChanged = true;
	apiComboBox.options.push_back("OpenGL");
	if (eg::VulkanAppearsSupported())
		apiComboBox.options.push_back("Vulkan");
	apiComboBox.getValue = [] { return (int)settings.graphicsAPI; };
	apiComboBox.setValue = [] (int value) { settings.graphicsAPI = (eg::GraphicsAPI)value; };
	
	std::span<std::string> gpuNames = eg::gal::GetDeviceNames();
	
	ComboBox gpuComboBox;
	gpuComboBox.label = "GPU";
	gpuComboBox.restartRequiredIfChanged = true;
	for (const std::string& deviceName : gpuNames)
	{
		gpuComboBox.options.emplace_back(FilterGPUNameString(deviceName));
	}
	gpuComboBox.getValue = [gpuNames]
	{
		int activeGpuIndex = 0;
		for (int i = 0; i < (int)gpuNames.size(); i++)
		{
			if (settings.preferredGPUName == gpuNames[i])
				return i;
			if (eg::GetGraphicsDeviceInfo().deviceName == gpuNames[i])
				activeGpuIndex = i;
		}
		return activeGpuIndex;
	};
	gpuComboBox.setValue = [gpuNames] (int value)
	{
		settings.preferredGPUName = gpuNames[value];
	};
	
	leftWidgetList.AddWidget(std::move(apiComboBox));
	leftWidgetList.AddWidget(std::move(gpuComboBox));
#endif
	
	leftWidgetList.AddWidget(SubtitleWidget("Graphics"));
	ComboBox textureQualityCB = InitSettingsCB(&Settings::textureQuality, "Textures", true);
	textureQualityCB.restartRequiredIfChanged = true;
	leftWidgetList.AddWidget(textureQualityCB);
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::shadowQuality, "Shadows", true));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::reflectionsQuality, "Reflections", true));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::lightingQuality, "Lighting", true));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::ssaoQuality, "SSAO", true));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::waterQuality, "Water", true));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::enableBloom, "Bloom", "On", "Off"));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::enableFXAA, "FXAA", "On", "Off"));
	leftWidgetList.AddWidget(InitSettingsToggleButton(&Settings::gunFlash, "Gun Flash", "Enabled", "Disabled"));
	leftWidgetList.AddWidget(InitSettingsCB(&Settings::viewBobbingLevel, "View Bobbing", true));
	Slider brighnessSlider = InitSettingsSlider(&Settings::exposure, "Brightness", 0.6f, 1.3f);
	brighnessSlider.getDisplayValueString = VolumeAndBrightnessGetDisplayValueString;
	leftWidgetList.AddWidget(std::move(brighnessSlider));
	Slider fovSlider = InitSettingsSlider(&Settings::fieldOfViewDeg, "Field of View", 60, 110);
	fovSlider.getDisplayValueString = [] (float value) -> std::string_view
	{
		static char buffer[16];
		snprintf(buffer, sizeof(buffer), "%d°", (int)std::round(value));
		return buffer;
	};
	leftWidgetList.AddWidget(std::move(fovSlider));
	
	rightWidgetList.AddWidget(SubtitleWidget("Audio"));
	rightWidgetList.AddWidget(InitVolumeSlider(&Settings::masterVolume, "Master Volume"));
	rightWidgetList.AddWidget(InitVolumeSlider(&Settings::sfxVolume, "SFX Volume"));
	rightWidgetList.AddWidget(InitVolumeSlider(&Settings::ambienceVolume, "Ambience Volume"));
	
	rightWidgetList.AddWidget(SubtitleWidget("Input"));
	rightWidgetList.AddWidget(InitSettingsToggleButton(&Settings::lookInvertY, "Look Invert Y", "Yes", "No"));
	rightWidgetList.AddWidget(InitSettingsSlider(&Settings::lookSensitivityMS, "Mouse Sensitivity", 0.001f, 0.02f));
	
	ComboBox joystickAxesCB;
	joystickAxesCB.label = "Joystick Mapping";
	joystickAxesCB.options = { "\eLeft: \eMove\e, Right:\e Look", "\eLeft: \eLook, \eRight:\e Move" };
	joystickAxesCB.getValue = [] { return (int)settings.flipJoysticks; };
	joystickAxesCB.setValue = [] (int value) { settings.flipJoysticks = value == 1; SettingsChanged(); };
	rightWidgetList.AddWidget(std::move(joystickAxesCB));
	rightWidgetList.AddWidget(InitSettingsSlider(&Settings::lookSensitivityGP, "Joystick Sensitivity", 0.5f, 4.0f));
	
	rightWidgetList.AddWidget(SubtitleWidget("Key Bindings"));
	ComboBox keyBindingsConfigureModeCB;
	keyBindingsConfigureModeCB.label = "Bindings to Configure";
	keyBindingsConfigureModeCB.options = { "Keyboard & Mouse", "Controller" };
	keyBindingsConfigureModeCB.getValue = [] { return (int)KeyBindingWidget::isConfiguringGamePad; };
	keyBindingsConfigureModeCB.setValue = [] (int value) { KeyBindingWidget::isConfiguringGamePad = value == 1; };
	rightWidgetList.AddWidget(std::move(keyBindingsConfigureModeCB));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Forward", settings.keyMoveF));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Backward", settings.keyMoveB));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Left", settings.keyMoveL));
	rightWidgetList.AddWidget(KeyBindingWidget("Move Right", settings.keyMoveR));
	rightWidgetList.AddWidget(KeyBindingWidget("Jump", settings.keyJump));
	rightWidgetList.AddWidget(KeyBindingWidget("Interact", settings.keyInteract));
	rightWidgetList.AddWidget(KeyBindingWidget("Shoot", settings.keyShoot));
	
	backButton = Button("Back", []
	{
		SaveSettings();
		UpdateDisplayMode();
		optionsMenuOpen = false;
	});
	backButton.width = 200;
}

EG_ON_INIT(InitOptionsMenu)

void UpdateOptionsMenu(float dt, const glm::vec2& positionOffset, bool allowInteraction)
{
	KeyBindingWidget::anyKeyBindingPickingKey = false;
	
	constexpr float Y_MARGIN_TOP = 50;
	constexpr float Y_MARGIN_BTM = 100;
	
	const glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	optionsScrollPanel.contentHeight = std::max(leftWidgetList.Height(), rightWidgetList.Height());
	
	optionsScrollPanel.screenRectangle.x =
		std::max(eg::CurrentResolutionX() / 2 - WIDGET_LIST_SPACING / 2 - WIDGET_LIST_WIDTH, 0.0f) + positionOffset.x;
	optionsScrollPanel.screenRectangle.w =
		std::min(WIDGET_LIST_SPACING + WIDGET_LIST_WIDTH * 2 + 20, (float)eg::CurrentResolutionX());
	optionsScrollPanel.screenRectangle.h =
		std::min(eg::CurrentResolutionY() - Y_MARGIN_TOP - Y_MARGIN_BTM, optionsScrollPanel.contentHeight);
	optionsScrollPanel.screenRectangle.y =
		std::max((eg::CurrentResolutionY() - optionsScrollPanel.screenRectangle.h) / 2, Y_MARGIN_BTM) + positionOffset.y;
	
	optionsScrollPanel.Update(dt, ComboBox::current == nullptr);
	
	leftWidgetList.position.x = optionsScrollPanel.screenRectangle.x;
	leftWidgetList.position.y = optionsScrollPanel.screenRectangle.MaxY() + optionsScrollPanel.scroll;
	rightWidgetList.position.x = (eg::CurrentResolutionX() + WIDGET_LIST_SPACING) / 2 + positionOffset.x;
	rightWidgetList.position.y = leftWidgetList.position.y;
	
	bool canInteractWidgetList = allowInteraction && optionsScrollPanel.screenRectangle.Contains(flippedCursorPos);
	leftWidgetList.Update(dt, canInteractWidgetList);
	rightWidgetList.Update(dt, canInteractWidgetList);
	
	backButton.position.x = (eg::CurrentResolutionX() - backButton.width) / 2 + positionOffset.x;
	backButton.position.y = optionsScrollPanel.screenRectangle.y - Button::height - 20 + positionOffset.y;
	backButton.Update(dt, allowInteraction);
}

void DrawOptionsMenu(eg::SpriteBatch& spriteBatch)
{
	spriteBatch.PushScissor(0, optionsScrollPanel.screenRectangle.y,
	                        eg::CurrentResolutionX(), optionsScrollPanel.screenRectangle.h);
	leftWidgetList.Draw(spriteBatch);
	rightWidgetList.Draw(spriteBatch);
	spriteBatch.PopScissor();
	
	backButton.Draw(spriteBatch);
	
	optionsScrollPanel.Draw(spriteBatch);
}
