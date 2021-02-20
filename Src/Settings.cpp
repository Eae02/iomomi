#include "Settings.hpp"
#include "AudioPlayers.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <magic_enum.hpp>

std::string settingsPath;

Settings settings;

void LoadSettings()
{
	settingsPath = eg::AppDataPath() + "EaeGravity/Settings.yaml";
	
	std::ifstream settingsStream(settingsPath);
	if (!settingsStream)
		return;
	
	YAML::Node settingsNode = YAML::Load(settingsStream);
	
	std::string textureQualStr = settingsNode["textureQuality"].as<std::string>("high");
	if (textureQualStr == "low")
		settings.textureQuality = eg::TextureQuality::Low;
	else if (textureQualStr == "medium")
		settings.textureQuality = eg::TextureQuality::Medium;
	else if (textureQualStr == "high")
		settings.textureQuality = eg::TextureQuality::High;
	
	settings.reflectionsQuality = magic_enum::enum_cast<QualityLevel>(settingsNode["reflQuality"].as<std::string>("")).value_or(QualityLevel::Medium);
	settings.shadowQuality      = magic_enum::enum_cast<QualityLevel>(settingsNode["shadowQuality"].as<std::string>("")).value_or(QualityLevel::Medium);
	settings.lightingQuality    = magic_enum::enum_cast<QualityLevel>(settingsNode["lightingQuality"].as<std::string>("")).value_or(QualityLevel::High);
	settings.waterQuality       = magic_enum::enum_cast<QualityLevel>(settingsNode["waterQuality"].as<std::string>("")).value_or(QualityLevel::High);
	settings.fieldOfViewDeg = settingsNode["fieldOfView"].as<float>(settings.fieldOfViewDeg);
	settings.exposure = settingsNode["exposure"].as<float>(settings.exposure);
	settings.lookSensitivityMS = settingsNode["lookSensitivityMS"].as<float>(settings.lookSensitivityMS);
	settings.lookSensitivityGP = settingsNode["lookSensitivityGP"].as<float>(settings.lookSensitivityGP);
	settings.lookInvertY = settingsNode["lookInvertY"].as<bool>(false);
	settings.enableFXAA = settingsNode["fxaa"].as<bool>(true);
	settings.enableBloom = settingsNode["bloom"].as<bool>(true);
	settings.showExtraLevels = settingsNode["showExtraLevels"].as<bool>(false);
	settings.gunFlash = settingsNode["gunFlash"].as<bool>(true);
	settings.drawCrosshair = settingsNode["crosshair"].as<bool>(true);
	
	settings.graphicsAPI = magic_enum::enum_cast<eg::GraphicsAPI>(settingsNode["graphicsAPI"].as<std::string>("")).value_or(eg::GraphicsAPI::OpenGL);
	settings.preferredGPUName = settingsNode["prefGPUName"].as<std::string>("");
	
	settings.displayMode = magic_enum::enum_cast<DisplayMode>(settingsNode["displayMode"].as<std::string>("")).value_or(DisplayMode::Windowed);
	settings.viewBobbingLevel = magic_enum::enum_cast<ViewBobbingLevel>(settingsNode["displayMode"].as<std::string>("")).value_or(ViewBobbingLevel::Normal);
	
	settings.fullscreenDisplayMode.resolutionX = settingsNode["resx"].as<uint32_t>(0);
	settings.fullscreenDisplayMode.resolutionY = settingsNode["resy"].as<uint32_t>(0);
	settings.fullscreenDisplayMode.refreshRate = settingsNode["refreshRate"].as<uint32_t>(0);
	settings.vsync = settingsNode["vsync"].as<bool>(true);
	
	settings.masterVolume = settingsNode["masterVolume"].as<float>(settings.masterVolume);
	settings.sfxVolume = settingsNode["sfxVolume"].as<float>(settings.sfxVolume);
	settings.ambienceVolume = settingsNode["ambienceVolume"].as<float>(settings.ambienceVolume);
	
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

void SaveSettings()
{
	YAML::Emitter emitter;
	
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "showExtraLevels"   << YAML::Value << settings.showExtraLevels;
	emitter << YAML::Key << "textureQuality"    << YAML::Value << std::string(magic_enum::enum_name(settings.textureQuality));
	emitter << YAML::Key << "reflQuality"       << YAML::Value << std::string(magic_enum::enum_name(settings.reflectionsQuality));
	emitter << YAML::Key << "shadowQuality"     << YAML::Value << std::string(magic_enum::enum_name(settings.shadowQuality));
	emitter << YAML::Key << "lightingQuality"   << YAML::Value << std::string(magic_enum::enum_name(settings.lightingQuality));
	emitter << YAML::Key << "waterQuality"      << YAML::Value << std::string(magic_enum::enum_name(settings.waterQuality));
	emitter << YAML::Key << "fieldOfView"       << YAML::Value << settings.fieldOfViewDeg;
	emitter << YAML::Key << "exposure"          << YAML::Value << settings.exposure;
	emitter << YAML::Key << "lookSensitivityMS" << YAML::Value << settings.lookSensitivityMS;
	emitter << YAML::Key << "lookSensitivityGP" << YAML::Value << settings.lookSensitivityGP;
	emitter << YAML::Key << "lookInvertY"       << YAML::Value << settings.lookInvertY;
	emitter << YAML::Key << "flipJoysticks"     << YAML::Value << settings.flipJoysticks;
	emitter << YAML::Key << "fxaa"              << YAML::Value << settings.enableFXAA;
	emitter << YAML::Key << "bloom"             << YAML::Value << settings.enableBloom;
	emitter << YAML::Key << "gunFlash"          << YAML::Value << settings.gunFlash;
	emitter << YAML::Key << "crosshair"         << YAML::Value << settings.drawCrosshair;
	emitter << YAML::Key << "viewBobbing"       << YAML::Value << std::string(magic_enum::enum_name(settings.viewBobbingLevel));
	
	emitter << YAML::Key << "displayMode" << YAML::Value << std::string(magic_enum::enum_name(settings.displayMode));
	emitter << YAML::Key << "resx"        << YAML::Value << settings.fullscreenDisplayMode.resolutionX;
	emitter << YAML::Key << "resy"        << YAML::Value << settings.fullscreenDisplayMode.resolutionY;
	emitter << YAML::Key << "refreshRate" << YAML::Value << settings.fullscreenDisplayMode.refreshRate;
	emitter << YAML::Key << "vsync"       << YAML::Value << settings.vsync;
	emitter << YAML::Key << "prefGPUName" << YAML::Value << settings.preferredGPUName;
	emitter << YAML::Key << "graphicsAPI" << YAML::Value << std::string(magic_enum::enum_name(settings.graphicsAPI));
	
	emitter << YAML::Key << "masterVolume"   << YAML::Value << settings.masterVolume;
	emitter << YAML::Key << "sfxVolume"      << YAML::Value << settings.sfxVolume;
	emitter << YAML::Key << "ambienceVolume" << YAML::Value << settings.ambienceVolume;
	
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
	
	eg::SetMasterVolume(settings.masterVolume);
	AudioPlayers::menuSFXPlayer.SetGlobalVolume(settings.sfxVolume);
	
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
