#pragma once

#include "Graphics/QualityLevel.hpp"

enum class DisplayMode
{
	Windowed,
	Fullscreen,
	FullscreenDesktop
};

struct KeyBinding
{
	eg::Button kbmButton;
	eg::Button controllerButton;
	eg::Button kbmDefault;
	eg::Button controllerDefault;
	
	KeyBinding(eg::Button kbm, eg::Button controller)
		: kbmButton(kbm), controllerButton(controller), kbmDefault(kbm), controllerDefault(controller) { }
	
	bool IsDown() { return eg::IsButtonDown(kbmButton) || eg::IsButtonDown(controllerButton); }
	bool WasDown() { return eg::WasButtonDown(kbmButton) || eg::WasButtonDown(controllerButton); }
};

struct Settings
{
	bool vsync = true;
	DisplayMode displayMode = DisplayMode::Windowed;
	eg::FullscreenDisplayMode fullscreenDisplayMode = {};
	
	bool showExtraLevels = false;
	
	eg::TextureQuality textureQuality = eg::TextureQuality::High;
	QualityLevel shadowQuality        = QualityLevel::High;
	QualityLevel reflectionsQuality   = QualityLevel::Medium;
	QualityLevel lightingQuality      = QualityLevel::High;
	QualityLevel waterQuality         = QualityLevel::Medium;
	float fieldOfViewDeg              = 80.0f;
	
	float exposure = 0.9f;
	
	bool enableFXAA         = true;
	bool enableBloom        = true;
	bool gunFlash           = true;
	
	KeyBinding keyMoveF { eg::Button::W, eg::Button::CtrlrDPadUp };
	KeyBinding keyMoveB { eg::Button::S, eg::Button::CtrlrDPadDown };
	KeyBinding keyMoveL { eg::Button::A, eg::Button::CtrlrDPadLeft };
	KeyBinding keyMoveR { eg::Button::D, eg::Button::CtrlrDPadRight };
	KeyBinding keyJump { eg::Button::Space, eg::Button::CtrlrA };
	KeyBinding keyInteract { eg::Button::E, eg::Button::CtrlrX };
	KeyBinding keyMenu { eg::Button::Escape, eg::Button::CtrlrStart };
	KeyBinding keyShoot { eg::Button::MouseLeft, eg::Button::CtrlrB };
	float lookSensitivityMS = 0.005f;
	float lookSensitivityGP = 2.0f;
	bool flipJoysticks      = false;
	bool lookInvertY        = false;
	
	bool HDREnabled() const
	{
		return lightingQuality >= QualityLevel::Low;
	}
	
	bool SSREnabled() const
	{
		return reflectionsQuality >= QualityLevel::Low;
	}
	
	bool BloomEnabled() const
	{
		return enableBloom && HDREnabled() && eg::GetGraphicsDeviceInfo().computeShader;
	}
};

extern Settings settings;

extern bool settingsWindowVisible;

void UpdateDisplayMode();

void DecodeQualityLevel(std::string_view name, QualityLevel& def);

int SettingsGeneration();

void LoadSettings();
void SaveSettings();

void SettingsChanged();
