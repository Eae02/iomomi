#include "Settings.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>

std::string settingsPath;

Settings settings;

void DecodeQualityLevel(std::string_view name, QualityLevel& def)
{
	if (name == "veryLow" || name == "vlow")
		def = QualityLevel::VeryLow;
	else if (name == "low")
		def = QualityLevel::Low;
	else if (name == "medium")
		def = QualityLevel::Medium;
	else if (name == "high")
		def = QualityLevel::High;
	else if (name == "veryHigh" || name == "vhigh")
		def = QualityLevel::VeryHigh;
	else
		eg::Log(eg::LogLevel::Error, "opt", "Unknwon quality level '{0}'", name);
}

static std::string displayModeNames[] = { "windowed", "fullscreen", "fullscreenDesktop" };

void LoadSettings()
{
	settingsPath = eg::AppDataPath() + "EaeGravity/Settings.yaml";
	
	std::ifstream settingsStream(settingsPath);
	if (!settingsStream)
		return;
	
	YAML::Node settingsNode = YAML::Load(settingsStream);
	
	std::string textureQualStr = settingsNode["textureQuality"].as<std::string>("medium");
	if (textureQualStr == "low")
		settings.textureQuality = eg::TextureQuality::Low;
	else if (textureQualStr == "medium")
		settings.textureQuality = eg::TextureQuality::Medium;
	else if (textureQualStr == "high")
		settings.textureQuality = eg::TextureQuality::High;
	
	DecodeQualityLevel(settingsNode["reflectionsQuality"].as<std::string>("medium"), settings.reflectionsQuality);
	DecodeQualityLevel(settingsNode["shadowQuality"].as<std::string>("medium"), settings.shadowQuality);
	DecodeQualityLevel(settingsNode["lightingQuality"].as<std::string>("medium"), settings.lightingQuality);
	DecodeQualityLevel(settingsNode["waterQuality"].as<std::string>("medium"), settings.waterQuality);
	settings.fieldOfViewDeg = settingsNode["fieldOfView"].as<float>(settings.fieldOfViewDeg);
	settings.exposure = settingsNode["exposure"].as<float>(settings.exposure);
	settings.lookSensitivityMS = settingsNode["lookSensitivityMS"].as<float>(settings.lookSensitivityMS);
	settings.lookSensitivityGP = settingsNode["lookSensitivityGP"].as<float>(settings.lookSensitivityGP);
	settings.lookInvertY = settingsNode["lookInvertY"].as<bool>(false);
	settings.enableFXAA = settingsNode["fxaa"].as<bool>(true);
	settings.enableBloom = settingsNode["bloom"].as<bool>(true);
	settings.showExtraLevels = settingsNode["showExtraLevels"].as<bool>(false);
	settings.gunFlash = settingsNode["gunFlash"].as<bool>(true);
	
	std::string displayModeString = settingsNode["displayMode"].as<std::string>("");
	for (int i = 0; i < 3; i++)
	{
		if (displayModeString == displayModeNames[i])
			settings.displayMode = (DisplayMode)i;
	}
	
	settings.fullscreenDisplayMode.resolutionX = settingsNode["resx"].as<uint32_t>(0);
	settings.fullscreenDisplayMode.resolutionY = settingsNode["resy"].as<uint32_t>(0);
	settings.fullscreenDisplayMode.refreshRate = settingsNode["refreshRate"].as<uint32_t>(0);
	settings.vsync = settingsNode["vsync"].as<bool>(true);
	
	auto ReadKeyBinding = [&] (const char* name, KeyBinding& binding)
	{
		std::string value = settingsNode[name].as<std::string>("");
		size_t spacePos = value.find(' ');
		if (spacePos != std::string::npos)
		{
			binding.kbmButton = eg::ButtonFromString(std::string_view(value.data(), spacePos));
			binding.controllerButton = eg::ButtonFromString(std::string_view(value.data() + spacePos + 1));
		}
	};
	ReadKeyBinding("keyMoveF", settings.keyMoveF);
	ReadKeyBinding("keyMoveB", settings.keyMoveB);
	ReadKeyBinding("keyMoveL", settings.keyMoveL);
	ReadKeyBinding("keyMoveR", settings.keyMoveR);
	ReadKeyBinding("keyJump", settings.keyJump);
	ReadKeyBinding("keyInteract", settings.keyInteract);
	ReadKeyBinding("keyMenu", settings.keyMenu);
	ReadKeyBinding("keyShoot", settings.keyShoot);
}

static const char* textureQualityNames[] = { "low", "medium", "high" };
static const char* qualityNames[] = { "veryLow", "low", "medium", "high", "veryHigh" };

void SaveSettings()
{
	YAML::Emitter emitter;
	
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "showExtraLevels" << YAML::Value << settings.showExtraLevels;
	emitter << YAML::Key << "textureQuality" << YAML::Value << textureQualityNames[(int)settings.textureQuality];
	emitter << YAML::Key << "reflectionsQuality" << YAML::Value << qualityNames[(int)settings.reflectionsQuality];
	emitter << YAML::Key << "shadowQuality" << YAML::Value << qualityNames[(int)settings.shadowQuality];
	emitter << YAML::Key << "lightingQuality" << YAML::Value << qualityNames[(int)settings.lightingQuality];
	emitter << YAML::Key << "waterQuality" << YAML::Value << qualityNames[(int)settings.waterQuality];
	emitter << YAML::Key << "fieldOfView" << YAML::Value << settings.fieldOfViewDeg;
	emitter << YAML::Key << "exposure" << YAML::Value << settings.exposure;
	emitter << YAML::Key << "lookSensitivityMS" << YAML::Value << settings.lookSensitivityMS;
	emitter << YAML::Key << "lookSensitivityGP" << YAML::Value << settings.lookSensitivityGP;
	emitter << YAML::Key << "lookInvertY" << YAML::Value << settings.lookInvertY;
	emitter << YAML::Key << "flipJoysticks" << YAML::Value << settings.flipJoysticks;
	emitter << YAML::Key << "fxaa" << YAML::Value << settings.enableFXAA;
	emitter << YAML::Key << "bloom" << YAML::Value << settings.enableBloom;
	emitter << YAML::Key << "gunFlash" << YAML::Value << settings.gunFlash;
	
	emitter << YAML::Key << "displayMode" << YAML::Value << displayModeNames[(int)settings.displayMode];
	emitter << YAML::Key << "resx" << YAML::Value << settings.fullscreenDisplayMode.resolutionX;
	emitter << YAML::Key << "resy" << YAML::Value << settings.fullscreenDisplayMode.resolutionY;
	emitter << YAML::Key << "refreshRate" << YAML::Value << settings.fullscreenDisplayMode.refreshRate;
	emitter << YAML::Key << "vsync" << YAML::Value << settings.vsync;
	
	auto WriteKeyBinding = [&] (const char* name, const KeyBinding& binding)
	{
		std::string value = eg::Concat({ eg::ButtonToString(binding.kbmButton), " ", eg::ButtonToString(binding.controllerButton) });
		emitter << YAML::Key << name << YAML::Value << value;
	};
	WriteKeyBinding("keyMoveF", settings.keyMoveF);
	WriteKeyBinding("keyMoveB", settings.keyMoveB);
	WriteKeyBinding("keyMoveL", settings.keyMoveL);
	WriteKeyBinding("keyMoveR", settings.keyMoveR);
	WriteKeyBinding("keyJump", settings.keyJump);
	WriteKeyBinding("keyInteract", settings.keyInteract);
	WriteKeyBinding("keyMenu", settings.keyMenu);
	WriteKeyBinding("keyShoot", settings.keyShoot);
	
	emitter << YAML::EndMap;
	
	std::ofstream settingsStream(settingsPath);
	if (settingsStream)
	{
		settingsStream << emitter.c_str();
	}
}

int settingsGeneration = 0;
std::optional<bool> prevVSyncState;

int SettingsGeneration()
{
	return settingsGeneration;
}

void SettingsChanged()
{
	if (!prevVSyncState.has_value() || *prevVSyncState != settings.vsync)
	{
		eg::gal::SetEnableVSync(settings.vsync);
		prevVSyncState = settings.vsync;
	}
	
	settingsGeneration++;
}

void UpdateDisplayMode()
{
	switch (settings.displayMode)
	{
	case DisplayMode::Windowed:
		eg::SetDisplayModeWindowed();
		break;
	case DisplayMode::Fullscreen:
		eg::SetDisplayModeFullscreen(settings.fullscreenDisplayMode);
		break;
	case DisplayMode::FullscreenDesktop:
		eg::SetDisplayModeFullscreenDesktop();
		break;
	}
}

static const eg::Button controllerButtons[] = 
{
	eg::Button::CtrlrA,
	eg::Button::CtrlrB,
	eg::Button::CtrlrX,
	eg::Button::CtrlrY,
	eg::Button::CtrlrBack,
	eg::Button::CtrlrGuide,
	eg::Button::CtrlrStart,
	eg::Button::CtrlrLeftStick,
	eg::Button::CtrlrRightStick,
	eg::Button::CtrlrLeftShoulder,
	eg::Button::CtrlrRightShoulder,
	eg::Button::CtrlrDPadUp,
	eg::Button::CtrlrDPadDown,
	eg::Button::CtrlrDPadLeft,
	eg::Button::CtrlrDPadRight,
};

bool IsControllerButton(eg::Button button)
{
	return eg::Contains(controllerButtons, button);
}
