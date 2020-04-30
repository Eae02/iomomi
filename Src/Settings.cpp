#include "Settings.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <imgui.h>

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

eg::TextureQuality initialTextureQuality;

void LoadSettings()
{
#ifndef __EMSCRIPTEN__
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
	initialTextureQuality = settings.textureQuality;
	
	DecodeQualityLevel(settingsNode["reflectionsQuality"].as<std::string>("medium"), settings.reflectionsQuality);
	DecodeQualityLevel(settingsNode["shadowQuality"].as<std::string>("medium"), settings.shadowQuality);
	DecodeQualityLevel(settingsNode["lightingQuality"].as<std::string>("medium"), settings.lightingQuality);
	DecodeQualityLevel(settingsNode["waterQuality"].as<std::string>("medium"), settings.waterQuality);
	settings.fieldOfViewDeg = settingsNode["fieldOfView"].as<float>(80.0f);
	settings.exposure = settingsNode["exposure"].as<float>(1.2f);
	settings.lookSensitivityMS = settingsNode["lookSensitivityMS"].as<float>(0.005f);
	settings.lookSensitivityGP = settingsNode["lookSensitivityGP"].as<float>(2.0f);
	settings.lookInvertY = settingsNode["lookInvertY"].as<bool>(false);
	settings.enableFXAA = settingsNode["fxaa"].as<bool>(true);
#endif
}

static const char* textureQualityNames[] = { "low", "medium", "high" };
static const char* qualityNames[] = { "veryLow", "low", "medium", "high", "veryHigh" };

void SaveSettings()
{
#ifndef __EMSCRIPTEN__
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
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
	emitter << YAML::Key << "fxaa" << YAML::Value << settings.enableFXAA;
	emitter << YAML::EndMap;
	
	std::ofstream settingsStream(settingsPath);
	if (settingsStream)
	{
		settingsStream << emitter.c_str();
	}
#endif
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
	else if (args[0] == "waterQuality")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Water Quality: {0}",
			        qualityLevelStrings[(int)settings.waterQuality]);
		}
		else
		{
			DecodeQualityLevel(args[1], settings.waterQuality);
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
	else if (args[0] == "exposure")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Exposure: {0}", settings.exposure);
		}
		else
		{
			settings.exposure = std::stof(std::string(args[1]));
			SettingsChanged();
		}
	}
	else if (args[0] == "lookSensitivityMS" || args[0] == "lookSensMS")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Mouse Sensitivity: {0}", settings.lookSensitivityMS);
		}
		else
		{
			settings.lookSensitivityMS = std::stof(std::string(args[1]));
			SettingsChanged();
		}
	}
	else if (args[0] == "lookSensitivityGP" || args[0] == "lookSensGP")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Gamepad Sensitivity: {0}", settings.lookSensitivityGP);
		}
		else
		{
			settings.lookSensitivityGP = std::stof(std::string(args[1]));
			SettingsChanged();
		}
	}
	else if (args[0] == "lookInvY")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "Invert Y: {0}", settings.lookInvertY);
		}
		else
		{
			settings.lookInvertY = args[1] == "true";
			SettingsChanged();
		}
	}
	else if (args[0] == "fxaa")
	{
		if (args.size() == 1)
		{
			eg::Log(eg::LogLevel::Info, "opt", "FXAA: {0}", settings.enableFXAA);
		}
		else
		{
			settings.enableFXAA = args[1] == "true";
			SettingsChanged();
		}
	}
}

static const char* commands[] =
{
	"reflQuality", "shadowQuality", "lightingQuality", "waterQuality", "fov", "exposure", "lookSensitivityMS",
	"lookSensMS", "lookSensitivityGP", "lookSensGP", "lookInvY", "fxaa"
};

static const char* qualityCommands[] =
{
	"reflQuality", "shadowQuality", "lightingQuality", "waterQuality"
};

void OptCommandCompleter1(eg::Span<const std::string_view> words, eg::console::CompletionsList& list)
{
	for (const char* cmd : commands)
		list.Add(cmd);
}

void OptCommandCompleter2(eg::Span<const std::string_view> words, eg::console::CompletionsList& list)
{
	if (eg::Contains(qualityCommands, words[1]))
	{
		for (const char* quality : qualityNames)
			list.Add(quality);
	}
}

bool settingsWindowVisible = false;

void OptWndCommand(eg::Span<const std::string_view> args)
{
	settingsWindowVisible = true;
}

void DrawSettingsWindow()
{
	if (!settingsWindowVisible)
		return;
	
	static const char* QUALITY_SETTINGS_STR = "Very Low\0Low\0Medium\0High\0Very High\0";
	static const char* TEXTURE_QUALITY_STR = "Low\0Medium\0High\0";
	
	ImGui::SetNextWindowSize(ImVec2(200, 250), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(100, 100), ImVec2(INFINITY, INFINITY));
	if (ImGui::Begin("Options", &settingsWindowVisible))
	{
		if (ImGui::CollapsingHeader("Graphics Quality", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Combo("Texture Quality", reinterpret_cast<int*>(&settings.textureQuality), TEXTURE_QUALITY_STR);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Requires restart");
			}
			
			if (ImGui::Combo("Shadow Quality", reinterpret_cast<int*>(&settings.shadowQuality), QUALITY_SETTINGS_STR))
				SettingsChanged();
			if (ImGui::Combo("Reflections Quality", reinterpret_cast<int*>(&settings.reflectionsQuality), QUALITY_SETTINGS_STR))
				SettingsChanged();
			if (ImGui::Combo("Lighting Quality", reinterpret_cast<int*>(&settings.lightingQuality), QUALITY_SETTINGS_STR))
				SettingsChanged();
			if (ImGui::Combo("Water Quality", reinterpret_cast<int*>(&settings.waterQuality), QUALITY_SETTINGS_STR))
				SettingsChanged();
		}
		
		if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::SliderFloat("Field of View", &settings.fieldOfViewDeg, 70.0f, 100.0f))
			{
				settings.fieldOfViewDeg = glm::clamp(settings.fieldOfViewDeg, 70.0f, 100.0f);
				SettingsChanged();
			}
			if (ImGui::SliderFloat("Exposure", &settings.exposure, 0.5f, 1.5f))
			{
				settings.exposure = std::max(settings.exposure, 0.5f);
				SettingsChanged();
			}
			ImGui::Checkbox("FXAA", &settings.enableFXAA);
		}
		
		if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::SliderFloat("Mouse Sensitivity", &settings.lookSensitivityMS, 0.001f, 0.01f))
				settings.lookSensitivityMS = std::max(settings.lookSensitivityMS, 0.0f);
			if (ImGui::SliderFloat("Gamepad Sensitivity", &settings.lookSensitivityGP, 0.1f, 4.0f))
				settings.lookSensitivityGP = std::max(settings.lookSensitivityGP, 0.0f);
			ImGui::Checkbox("Look Invert Y", &settings.lookInvertY);
		}
	}
	ImGui::End();
}
