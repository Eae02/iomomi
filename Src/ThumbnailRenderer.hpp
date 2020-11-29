#pragma once

constexpr int LEVEL_THUMBNAIL_RES_X = 450;
constexpr int LEVEL_THUMBNAIL_RES_Y = 360;

struct LevelThumbnailUpdate;

LevelThumbnailUpdate* BeginUpdateLevelThumbnails(struct RenderContext& renderContext, eg::console::Writer& writer);

void EndUpdateLevelThumbnails(LevelThumbnailUpdate* update);
