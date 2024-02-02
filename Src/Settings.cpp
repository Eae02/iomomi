#include "Settings.hpp"

#include <charconv>
#include <fstream>
#include <magic_enum/magic_enum.hpp>

#include "AudioPlayers.hpp"
#include "FileUtils.hpp"

std::string settingsPath;

Settings settings;

struct SettingEntry
{
	std::string name;
	std::function<void(Settings& settings, std::string_view value)> load;
	std::function<std::string(const Settings& settings)> save;
};

template <typename T>
SettingEntry MakeEnumSetting(std::string name, T Settings::*setting)
{
	SettingEntry s;
	s.load = [=](Settings& st, std::string_view value)
	{
		if (std::optional<T> parsedValue = magic_enum::enum_cast<T>(value))
		{
			st.*setting = *parsedValue;
		}
	};
	s.save = [=](const Settings& st) { return std::string(magic_enum::enum_name(st.*setting)); };
	s.name = std::move(name);
	return s;
}

template <typename T>
SettingEntry MakeNumberSetting(std::string name, T Settings::*setting)
{
	SettingEntry s;
	s.load = [=](Settings& st, std::string_view str)
	{
		if constexpr (std::is_floating_point_v<T> && __clang__)
		{
			std::string copyBecauseClangIsBad(str);
			st.*setting = std::stod(copyBecauseClangIsBad);
		}
		else
		{
			std::from_chars(str.data(), str.data() + str.size(), st.*setting);
		}
	};
	s.save = [=](const Settings& st) { return std::to_string(st.*setting); };
	s.name = std::move(name);
	return s;
}

SettingEntry MakeStringSetting(std::string name, std::string Settings::*setting)
{
	SettingEntry s;
	s.load = [=](Settings& st, std::string_view str) { st.*setting = std::string(str); };
	s.save = [=](const Settings& st) -> std::string { return st.*setting; };
	s.name = std::move(name);
	return s;
}

SettingEntry MakeBoolSetting(std::string name, bool Settings::*setting)
{
	SettingEntry s;
	s.load = [=](Settings& st, std::string_view str)
	{
		if (str == "true")
			st.*setting = true;
		else if (str == "false")
			st.*setting = false;
	};
	s.save = [=](const Settings& st) { return st.*setting ? "true" : "false"; };
	s.name = std::move(name);
	return s;
}

SettingEntry MakeDisplayModeSetting(std::string name, uint32_t eg::FullscreenDisplayMode::*setting)
{
	SettingEntry s;
	s.load = [=](Settings& st, std::string_view str)
	{ std::from_chars(str.data(), str.data() + str.size(), st.fullscreenDisplayMode.*setting); };
	s.save = [=](const Settings& st) { return std::to_string(st.fullscreenDisplayMode.*setting); };
	s.name = std::move(name);
	return s;
}

SettingEntry MakeKeySetting(std::string name, KeyBinding Settings::*setting)
{
	SettingEntry s;
	s.load = [=](Settings& st, std::string_view value)
	{
		size_t spacePos = value.find(' ');
		if (spacePos != std::string::npos)
		{
			(st.*setting).kbmButton = eg::ButtonFromString(value.substr(0, spacePos));
			(st.*setting).controllerButton = eg::ButtonFromString(value.substr(spacePos + 1));
		}
	};
	s.save = [=](const Settings& st)
	{
		return eg::Concat(
			{ eg::ButtonToString((st.*setting).kbmButton), " ", eg::ButtonToString((st.*setting).controllerButton) });
	};
	s.name = std::move(name);
	return s;
}

const SettingEntry settingEntries[] = {
	MakeEnumSetting("textureQuality", &Settings::textureQuality),
	MakeEnumSetting("reflQuality", &Settings::reflectionsQuality),
	MakeEnumSetting("shadowQuality", &Settings::shadowQuality),
	MakeEnumSetting("lightingQuality", &Settings::lightingQuality),
	MakeEnumSetting("waterQuality", &Settings::waterQuality),
	MakeEnumSetting("ssaoQuality", &Settings::ssaoQuality),
	MakeEnumSetting("bloomQuality", &Settings::bloomQuality),
	MakeNumberSetting("anisotropicFiltering", &Settings::anisotropicFiltering),

	MakeBoolSetting("showExtraLevels", &Settings::showExtraLevels),
	MakeNumberSetting("fieldOfView", &Settings::fieldOfViewDeg),
	MakeNumberSetting("exposure", &Settings::exposure),
	MakeNumberSetting("lookSensitivityMS", &Settings::lookSensitivityMS),
	MakeNumberSetting("lookSensitivityGP", &Settings::lookSensitivityGP),
	MakeBoolSetting("lookInvertY", &Settings::lookInvertY),
	MakeBoolSetting("flipJoysticks", &Settings::flipJoysticks),
	MakeBoolSetting("fxaa", &Settings::enableFXAA),
	MakeBoolSetting("gunFlash", &Settings::gunFlash),
	MakeBoolSetting("crosshair", &Settings::drawCrosshair),
	MakeEnumSetting("viewBobbing", &Settings::viewBobbingLevel),

	MakeDisplayModeSetting("resx", &eg::FullscreenDisplayMode::resolutionX),
	MakeDisplayModeSetting("resy", &eg::FullscreenDisplayMode::resolutionY),
	MakeDisplayModeSetting("refreshRate", &eg::FullscreenDisplayMode::refreshRate),
	MakeBoolSetting("vsync", &Settings::vsync),
	MakeStringSetting("prefGPUName", &Settings::preferredGPUName),
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
	const bool maybeSlowGPU = eg::GetGraphicsDeviceInfo().deviceVendorName.find("Intel") != std::string_view::npos ||
	                          eg::GetGraphicsDeviceInfo().deviceName.find("Intel") != std::string_view::npos;
	if (maybeSlowGPU)
	{
		settings.reflectionsQuality = QualityLevel::VeryLow;
	}

	settingsPath = appDataDirPath + "settings.yaml";

	std::ifstream settingsStream(settingsPath, std::ios::binary);
	if (!settingsStream)
		return;

	std::string line;
	while (std::getline(settingsStream, line))
	{
		size_t colPos = line.find(':');
		if (colPos == std::string::npos)
			continue;

		std::string_view name(line.data(), colPos);

		for (const SettingEntry& entry : settingEntries)
		{
			if (entry.name == name)
			{
				entry.load(settings, eg::TrimString(line.substr(colPos + 1)));
				break;
			}
		}
	}
}

void SaveSettings()
{
	std::ofstream settingsStream(settingsPath, std::ios::binary);
	if (!settingsStream)
		return;

	for (const SettingEntry& entry : settingEntries)
	{
		settingsStream << entry.name << ": " << entry.save(settings) << "\n";
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

static const eg::Button controllerButtons[] = {
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
