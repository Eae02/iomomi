#pragma once

#include "Graphics/QualityLevel.hpp"
#include "World/Entities/EntityTypes.hpp"

enum class DisplayMode
{
	Windowed,
	Fullscreen,
	FullscreenDesktop
};

enum class ViewBobbingLevel
{
	Off,
	Low,
	Normal
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
	QualityLevel shadowQuality        = QualityLevel::Medium;
	QualityLevel reflectionsQuality   = QualityLevel::Medium;
	QualityLevel lightingQuality      = QualityLevel::High;
	QualityLevel waterQuality         = QualityLevel::High;
	float fieldOfViewDeg              = 80.0f;
	
	eg::GraphicsAPI graphicsAPI = eg::GraphicsAPI::OpenGL;
	std::string preferredGPUName;
	
	float exposure = 0.9f;
	
	bool enableFXAA         = true;
	bool enableBloom        = true;
	bool gunFlash           = true;
	bool drawCrosshair      = true;
	
	ViewBobbingLevel viewBobbingLevel = ViewBobbingLevel::Normal;
	
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
	
	float masterVolume   = 1.0f;
	float sfxVolume      = 1.0f;
	float ambienceVolume = 0.6f;
	
	std::array<bool, (int)EntTypeID::MAX> edEntityIconEnabled;
	
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

int SettingsGeneration();

void LoadSettings();
void SaveSettings();

void SettingsChanged();

bool IsControllerButton(eg::Button button);
