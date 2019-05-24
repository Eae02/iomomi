#pragma once

#include "Graphics/QualityLevel.hpp"

struct Settings
{
	eg::TextureQuality textureQuality = eg::TextureQuality::Medium;
	QualityLevel shadowQuality = QualityLevel::Medium;
	QualityLevel reflectionsQuality = QualityLevel::Medium;
	QualityLevel lightingQuality = QualityLevel::High;
	float fieldOfViewDeg = 80.0f;
	uint32_t msaaSamples = 1;
	
	float exposure = 1.2f;
	
	float lookSensitivityMS = 0.005f;
	float lookSensitivityGP = 2.0f;
	bool lookInvertY = false;
	
	bool HDREnabled() const
	{
		return lightingQuality >= QualityLevel::Low;
	}
	
	bool BloomEnabled() const
	{
		return lightingQuality >= QualityLevel::Medium;
	}
};

extern Settings settings;

int SettingsGeneration();

void LoadSettings();

void SettingsChanged();

void OptCommand(eg::Span<const std::string_view> args);
