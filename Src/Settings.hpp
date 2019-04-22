#pragma once

#include "Graphics/QualityLevel.hpp"

struct Settings
{
	eg::TextureQuality textureQuality = eg::TextureQuality::Medium;
	QualityLevel shadowQuality = QualityLevel::Medium;
	QualityLevel reflectionsQuality = QualityLevel::Medium;
	QualityLevel lightingQuality = QualityLevel::High;
	float fieldOfViewDeg = 80.0f;
	
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
