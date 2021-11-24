#include "Settings.hpp"
#include "AudioPlayers.hpp"
#include "FileUtils.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <magic_enum.hpp>

std::string settingsPath;

Settings settings;

struct SettingEntry
{
	std::function<void(Settings& settings, const YAML::Node& settingsNode)> load;
	std::function<void(const Settings& settings, YAML::Emitter& emitter)> save;
};

template <typename T>
SettingEntry MakeEnumSetting(std::string name, T Settings::*setting)
{
	SettingEntry s;
	s.load = [=] (Settings& st, const YAML::Node& settingsNode)
	{
		if (std::optional<T> value = magic_enum::enum_cast<T>(settingsNode[name].as<std::string>("")))
		{
			st.*setting = *value;
		}
	};
	s.save = [=] (const Settings& st, YAML::Emitter& emitter)
	{
		emitter << YAML::Key << name << YAML::Value << std::string(magic_enum::enum_name(st.*setting));
	};
	return s;
}

template <typename T>
SettingEntry MakeNumberSetting(std::string name, T Settings::*setting)
{
	SettingEntry s;
	s.load = [=] (Settings& st, const YAML::Node& settingsNode)
	{
		st.*setting = settingsNode[name].as<T>(st.*setting);
	};
	s.save = [=] (const Settings& st, YAML::Emitter& emitter)
	{
		emitter << YAML::Key << name << YAML::Value << st.*setting;
	};
	return s;
}

SettingEntry MakeDisplayModeSetting(std::string name, uint32_t eg::FullscreenDisplayMode::*setting)
{
	SettingEntry s;
	s.load = [=] (Settings& st, const YAML::Node& settingsNode)
	{
		st.fullscreenDisplayMode.*setting = settingsNode[name].as<uint32_t>(st.fullscreenDisplayMode.*setting);
	};
	s.save = [=] (const Settings& st, YAML::Emitter& emitter)
	{
		emitter << YAML::Key << name << YAML::Value << st.fullscreenDisplayMode.*setting;
	};
	return s;
}

SettingEntry MakeKeySetting(std::string name, KeyBinding Settings::*setting)
{
	SettingEntry s;
	s.load = [=] (Settings& st, const YAML::Node& settingsNode)
	{
		std::string value = settingsNode[name].as<std::string>("");
		size_t spacePos = value.find(' ');
		if (spacePos != std::string::npos)
		{
			(st.*setting).kbmButton = eg::ButtonFromString(std::string_view(value.data(), spacePos));
			(st.*setting).controllerButton = eg::ButtonFromString(std::string_view(value.data() + spacePos + 1));
		}
	};
	s.save = [=] (const Settings& st, YAML::Emitter& emitter)
	{
		std::string value = eg::Concat({
			eg::ButtonToString((st.*setting).kbmButton), " ",
			eg::ButtonToString((st.*setting).controllerButton)
		});
		emitter << YAML::Key << name << YAML::Value << value;
	};
	return s;
}

const SettingEntry settingEntries[] =
{
	MakeEnumSetting("textureQuality", &Settings::textureQuality),
	MakeEnumSetting("reflQuality", &Settings::reflectionsQuality),
	MakeEnumSetting("shadowQuality", &Settings::shadowQuality),
	MakeEnumSetting("lightingQuality", &Settings::lightingQuality),
	MakeEnumSetting("waterQuality", &Settings::waterQuality),
	MakeEnumSetting("ssaoQuality", &Settings::ssaoQuality),
	MakeEnumSetting("bloomQuality", &Settings::bloomQuality),
	
	MakeNumberSetting("showExtraLevels", &Settings::showExtraLevels),
	MakeNumberSetting("fieldOfView", &Settings::fieldOfViewDeg),
	MakeNumberSetting("exposure", &Settings::exposure),
	MakeNumberSetting("lookSensitivityMS", &Settings::lookSensitivityMS),
	MakeNumberSetting("lookSensitivityGP", &Settings::lookSensitivityGP),
	MakeNumberSetting("lookInvertY", &Settings::lookInvertY),
	MakeNumberSetting("flipJoysticks", &Settings::flipJoysticks),
	MakeNumberSetting("fxaa", &Settings::enableFXAA),
	MakeNumberSetting("gunFlash", &Settings::gunFlash),
	MakeNumberSetting("crosshair", &Settings::drawCrosshair),
	MakeEnumSetting("viewBobbing", &Settings::viewBobbingLevel),
	
	MakeDisplayModeSetting("resx", &eg::FullscreenDisplayMode::resolutionX),
	MakeDisplayModeSetting("resy", &eg::FullscreenDisplayMode::resolutionY),
	MakeDisplayModeSetting("refreshRate", &eg::FullscreenDisplayMode::refreshRate),
	MakeNumberSetting("vsync", &Settings::vsync),
	MakeNumberSetting("prefGPUName", &Settings::preferredGPUName),
	MakeEnumSetting("displayMode", &Settings::displayMode),
	MakeEnumSetting("graphicsAPI", &Settings::graphicsAPI),
	
	MakeNumberSetting("masterVolume", &Settings::masterVolume),
	MakeNumberSetting("sfxVolume", &Settings::sfxVolume),
	MakeNumberSetting("ambienceVolume", &Settings::ambienceVolume),
	
	MakeKeySetting("keyMoveF", &Settings::keyMoveF),
	MakeKeySetting("keyMoveB", &Settings::keyMoveB),
	MakeKeySetting("keyMoveL", &Settings::keyMoveL),
	MakeKeySetting("keyMoveR", &Settings::keyMoveR),
	MakeKeySetting("keyJump", &Settings::keyJump),
	MakeKeySetting("keyInteract", &Settings::keyInteract),
	MakeKeySetting("keyMenu", &Settings::keyMenu),
	MakeKeySetting("keyShoot", &Settings::keyShoot),
};

void LoadSettings()
{
#ifdef __EMSCRIPTEN__
	settings.textureQuality = eg::TextureQuality::Low;
	settings.shadowQuality = QualityLevel::Low;
	settings.reflectionsQuality = QualityLevel::VeryLow;
	settings.lightingQuality = QualityLevel::Medium;
	settings.ssaoQuality = SSAOQuality::Off;
	settings.bloomQuality = BloomQuality::Off;
#endif
	
	settingsPath = appDataDirPath + "settings.yaml";
	
	std::ifstream settingsStream(settingsPath);
	if (!settingsStream)
		return;
	
	YAML::Node settingsNode = YAML::Load(settingsStream);
	
	for (const SettingEntry& entry : settingEntries)
	{
		entry.load(settings, settingsNode);
	}
}

void SaveSettings()
{
	YAML::Emitter emitter;
	
	emitter << YAML::BeginMap;
	
	for (const SettingEntry& entry : settingEntries)
	{
		entry.save(settings, emitter);
	}
	
	emitter << YAML::EndMap;
	
	std::ofstream settingsStream(settingsPath);
	if (settingsStream)
	{
		settingsStream << emitter.c_str();
	}
	settingsStream.close();
	
	SyncFileSystem();
}

int settingsGeneration = 0;
std::optional<bool> prevVSyncState;

int SettingsGeneration()
{
	return settingsGeneration;
}

void UpdateVolumeSettings()
{
	eg::SetMasterVolume(settings.masterVolume);
	AudioPlayers::menuSFXPlayer.SetGlobalVolume(settings.sfxVolume);
	AudioPlayers::gameSFXPlayer.SetGlobalVolume(settings.sfxVolume);
}

void SettingsChanged()
{
	if (!prevVSyncState.has_value() || *prevVSyncState != settings.vsync)
	{
		eg::gal::SetEnableVSync(settings.vsync);
		prevVSyncState = settings.vsync;
	}
	UpdateVolumeSettings();
	
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
