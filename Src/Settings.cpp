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
	settings.fieldOfViewDeg = settingsNode["fieldOfView"].as<float>(80.0f);
}

int settingsGeneration = 0;

int SettingsGeneration()
{
	return settingsGeneration;
}

void SettingsChanged()
{
	settingsGeneration++;
}

void OptCommand(eg::Span<const std::string_view> args)
{
	std::string qualityLevelStrings[] = { "Very Low", "Low", "Medium", "High", "Very High" };
	
	if (args[0] == "reflQuality")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Reflections Quality: {0}",
			        qualityLevelStrings[(int)settings.reflectionsQuality]);
		}
		else
		{
			DecodeQualityLevel(args[1], settings.reflectionsQuality);
			SettingsChanged();
		}
	}
	else if (args[0] == "shadowQuality")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Shadow Quality: {0}",
			        qualityLevelStrings[(int)settings.shadowQuality]);
		}
		else
		{
			DecodeQualityLevel(args[1], settings.shadowQuality);
			SettingsChanged();
		}
	}
	else if (args[0] == "lightingQuality")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Lighting Quality: {0}",
			        qualityLevelStrings[(int)settings.lightingQuality]);
		}
		else
		{
			DecodeQualityLevel(args[1], settings.lightingQuality);
			SettingsChanged();
		}
	}
	else if (args[0] == "fov")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Field of View: {0}", settings.fieldOfViewDeg);
		}
		else
		{
			settings.fieldOfViewDeg = glm::clamp(std::stof(std::string(args[1])), 60.0f, 90.0f);
			SettingsChanged();
		}
	}
}
