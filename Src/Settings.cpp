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

static std::string displayModeNames[] = { "windowed", "fullscreen", "fullscreenDesktop" };

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
	settings.enableBloom = settingsNode["bloom"].as<bool>(true);
	settings.showExtraLevels = settingsNode["showExtraLevels"].as<bool>(false);
	
	std::string displayModeString = settingsNode["displayMode"].as<std::string>("");
	for (int i = 0; i < 3; i++)
	{
		if (displayModeString == displayModeNames[i])
			settings.displayMode = (DisplayMode)i;
	}
	
	settings.fullscreenDisplayMode.resolutionX = settingsNode["resx"].as<uint32_t>(0);
	settings.fullscreenDisplayMode.resolutionY = settingsNode["resy"].as<uint32_t>(0);
	settings.fullscreenDisplayMode.refreshRate = settingsNode["refreshRate"].as<uint32_t>(0);
	settings.vsync = settingsNode["vsync"].as<bool>(true);
	
#endif
}

static const char* textureQualityNames[] = { "low", "medium", "high" };
static const char* qualityNames[] = { "veryLow", "low", "medium", "high", "veryHigh" };

void SaveSettings()
{
#ifndef __EMSCRIPTEN__
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "showExtraLevels" << YAML::Value << settings.showExtraLevels;
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
	emitter << YAML::Key << "bloom" << YAML::Value << settings.enableBloom;
	
	emitter << YAML::Key << "displayMode" << YAML::Value << displayModeNames[(int)settings.displayMode];
	emitter << YAML::Key << "resx" << YAML::Value << settings.fullscreenDisplayMode.resolutionX;
	emitter << YAML::Key << "resy" << YAML::Value << settings.fullscreenDisplayMode.resolutionY;
	emitter << YAML::Key << "refreshRate" << YAML::Value << settings.fullscreenDisplayMode.refreshRate;
	emitter << YAML::Key << "vsync" << YAML::Value << settings.vsync;
	
	emitter << YAML::EndMap;
	
	std::ofstream settingsStream(settingsPath);
	if (settingsStream)
	{
		settingsStream << emitter.c_str();
	}
#endif
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
	
	settingsGeneration++;
}

void OptCommand(eg::Span<const std::string_view> args, eg::console::Writer& writer)
{
	std::string qualityLevelStrings[] = { "Very Low", "Low", "Medium", "High", "Very High" };
	
	auto WriteSettingValue = [&] (std::string_view message, std::string_view value)
	{
		writer.Write(eg::console::InfoColor, message);
		writer.WriteLine(eg::console::InfoColorSpecial, value);
	};
	
	if (args[0] == "reflQuality")
	{
		if (args.size() == 1)
		{
			WriteSettingValue("Reflections Quality: ", qualityLevelStrings[(int)settings.reflectionsQuality]);
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
			WriteSettingValue("Shadow Quality: ", qualityLevelStrings[(int)settings.shadowQuality]);
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
			WriteSettingValue("Lighting Quality: ", qualityLevelStrings[(int)settings.lightingQuality]);
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
			WriteSettingValue("Water Quality: ", qualityLevelStrings[(int)settings.waterQuality]);
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
			WriteSettingValue("Field of View: ", std::to_string(settings.fieldOfViewDeg));
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
			WriteSettingValue("Exposure: ", std::to_string(settings.exposure));
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
			WriteSettingValue("Mouse Sensitivity: ", std::to_string(settings.lookSensitivityMS));
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
			WriteSettingValue("Gamepad Sensitivity: ", std::to_string(settings.lookSensitivityGP));
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
			WriteSettingValue("Invert Y: ", settings.lookInvertY ? "yes" : "no");
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
			WriteSettingValue("FXAA: ", settings.enableFXAA ? "on" : "off");
		}
		else
		{
			settings.enableFXAA = args[1] == "true";
			SettingsChanged();
		}
	}
	else if (args[0] == "bloom")
	{
		if (args.size() == 1)
		{
			WriteSettingValue("BLOOM: ", settings.enableBloom ? "on" : "off");
		}
		else
		{
			settings.enableBloom = args[1] == "true";
			SettingsChanged();
		}
	}
}

static const char* commands[] =
{
	"reflQuality", "shadowQuality", "lightingQuality", "waterQuality", "fov", "exposure", "lookSensitivityMS",
	"lookSensMS", "lookSensitivityGP", "lookSensGP", "lookInvY", "fxaa", "bloom"
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

void OptWndCommand(eg::Span<const std::string_view> args, eg::console::Writer&)
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
			ImGui::Checkbox("Bloom", &settings.enableBloom);
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
