#pragma once

constexpr int LEVEL_THUMBNAIL_RES_X = 448;
constexpr int LEVEL_THUMBNAIL_RES_Y = 360;
constexpr float LEVEL_THUMBNAIL_FOV = 80.0f;

struct LevelThumbnailUpdate;

LevelThumbnailUpdate* BeginUpdateLevelThumbnails(eg::console::Writer& writer);

void EndUpdateLevelThumbnails(LevelThumbnailUpdate* update);
