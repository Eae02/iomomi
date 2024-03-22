#pragma once

#include <string_view>

#include "World/World.hpp"

enum class LevelStatus
{
	Locked,
	Unlocked,
	Completed
};

struct Level
{
	std::string name;
	LevelStatus status = LevelStatus::Unlocked;
	bool isExtra = true;
	int64_t nextLevelIndex = -1;
	eg::Texture thumbnail;

	explicit Level(std::string _name = "") : name(std::move(_name)) {}
};

extern std::vector<Level> levels;

extern std::vector<std::string_view> levelsOrder;

std::unique_ptr<World> LoadLevelWorld(const Level& level, bool isEditor);

void MarkLevelCompleted(Level& level);

void InitLevels();

void ResetProgress();
void SaveProgress();
void WriteProgressToStream(std::ostream& stream);

void LoadLevelThumbnail(Level& level);

std::string GetLevelPath(std::string_view name);
std::string GetLevelThumbnailPath(std::string_view name);

int64_t FindLevel(std::string_view name);

void SortLevels();

bool IsLevelLoadingComplete(std::string_view name);

class LevelsGraph
{
public:
	struct LevelConnectionNode;

	using Node = std::variant<LevelConnectionNode>;

	struct LevelConnectionNode
	{
		std::string_view levelName;
		std::string_view gateName;
		std::array<const Node*, 1> neighbors{};
	};

	static std::optional<LevelsGraph> LoadFromFile();

	static LevelsGraph Load(std::string_view description);

	const Node* FindNodeByLevelConnection(std::string_view levelName, std::string_view gateName) const;

private:
	struct LevelConnectionKey
	{
		std::string_view levelName;
		std::string_view gateName;

		size_t Hash() const;

		bool operator==(const LevelConnectionKey&) const = default;
		bool operator!=(const LevelConnectionKey&) const = default;
	};

	eg::LinearAllocator m_allocator;

	std::unordered_map<LevelConnectionKey, Node*, eg::MemberFunctionHash<LevelConnectionKey>> m_levelConnectionNodeMap;
};
