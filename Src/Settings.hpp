#pragma once

#include "Graphics/QualityLevel.hpp"

struct Settings
{
	bool fullscreen = false;
	int fullscreenResX = -1;
	int fullscreenResY = -1;
	
	eg::TextureQuality textureQuality = eg::TextureQuality::High;
	QualityLevel shadowQuality        = QualityLevel::Medium;
	QualityLevel reflectionsQuality   = QualityLevel::Medium;
	QualityLevel lightingQuality      = QualityLevel::Medium;
	QualityLevel waterQuality         = QualityLevel::Medium;
	float fieldOfViewDeg              = 80.0f;
	
	float exposure = 1.2f;
	
	float lookSensitivityMS = 0.005f;
	float lookSensitivityGP = 2.0f;
	bool lookInvertY        = false;
	bool enableFXAA         = true;
	bool enableBloom        = false;
	
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

void DecodeQualityLevel(std::string_view name, QualityLevel& def);

int SettingsGeneration();

void LoadSettings();
void SaveSettings();

void SettingsChanged();

void OptCommand(eg::Span<const std::string_view> args, eg::console::Writer& writer);

void OptCommandCompleter1(eg::Span<const std::string_view> words, eg::console::CompletionsList& list);
void OptCommandCompleter2(eg::Span<const std::string_view> words, eg::console::CompletionsList& list);

void OptWndCommand(eg::Span<const std::string_view> args, eg::console::Writer& writer);

void DrawSettingsWindow();
