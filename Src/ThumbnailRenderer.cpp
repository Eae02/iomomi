#include "ThumbnailRenderer.hpp"
#include "Levels.hpp"
#include "Graphics/RenderContext.hpp"
#include "World/World.hpp"
#include "GameRenderer.hpp"

#include <fstream>
#include <EGame/Graphics/ImageWriter.hpp>

struct LevelThumbnailEntry
{
	std::string thumbnailPath;
	eg::Texture texture;
	eg::Framebuffer framebuffer;
	eg::Buffer downloadBuffer;
	Level* level;
};

struct LevelThumbnailUpdate
{
	std::vector<LevelThumbnailEntry> thumbnails;
};

constexpr size_t THUMBNAIL_BYTES = LEVEL_THUMBNAIL_RES_X * LEVEL_THUMBNAIL_RES_Y * 4;

LevelThumbnailUpdate* BeginUpdateLevelThumbnails(RenderContext& renderContext, eg::console::Writer& writer)
{
	std::unique_ptr<GameRenderer> renderer;
	LevelThumbnailUpdate* update = nullptr;
	
	eg::TextureCreateInfo textureCI;
	textureCI.width = LEVEL_THUMBNAIL_RES_X;
	textureCI.height = LEVEL_THUMBNAIL_RES_Y;
	textureCI.depth = 1;
	textureCI.mipLevels = 1;
	textureCI.flags = eg::TextureFlags::CopySrc | eg::TextureFlags::FramebufferAttachment;
	textureCI.format = eg::Format::R8G8B8A8_sRGB;
	
	eg::Profiler* profiler = eg::Profiler::current;
	eg::Profiler::current = nullptr;
	
	int numUpdated = 0;
	
	eg::CommandContext cc;
	
	for (Level& level : levels)
	{
		std::string levelPath = GetLevelPath(level.name);
		std::string thumbnailPath = GetLevelThumbnailPath(level.name);
		if (eg::FileExists(thumbnailPath.c_str()) && eg::LastWriteTime(thumbnailPath.c_str()) >= eg::LastWriteTime(levelPath.c_str()))
		{
			continue;
		}
		
		if (renderer == nullptr)
		{
			renderer = std::make_unique<GameRenderer>(renderContext);
			update = new LevelThumbnailUpdate; 
		}
		
		std::ifstream worldStream(levelPath, std::ios::binary);
		if (!worldStream)
		{
			std::string errorMessage = "Error opening file for reading: " + levelPath;
			writer.WriteLine(eg::console::ErrorColor, errorMessage);
			continue;
		}
		std::unique_ptr<World> world = World::Load(worldStream, false);
		if (world == nullptr)
			continue;
		worldStream.close();
		
		renderer->WorldChanged(*world);
		
		WorldUpdateArgs updateArgs = {};
		updateArgs.mode = WorldMode::Thumbnail;
		updateArgs.world = world.get();
		updateArgs.waterSim = &renderer->m_waterSimulator;
		world->Update(updateArgs, nullptr);
		
		renderer->SetViewMatrixFromThumbnailCamera(*world);
		
		LevelThumbnailEntry& entry = update->thumbnails.emplace_back();
		entry.thumbnailPath = std::move(thumbnailPath);
		entry.level = &level;
		
		std::string textureLabel = "ThumbnalDst[" + level.name + "]";
		textureCI.label = textureLabel.c_str();
		entry.texture = eg::Texture::Create2D(textureCI);
		eg::FramebufferAttachment framebufferAttachment;
		framebufferAttachment.texture = entry.texture.handle;
		entry.framebuffer = eg::Framebuffer(eg::Span<const eg::FramebufferAttachment>(&framebufferAttachment, 1));
		
		renderer->m_waterSimulator.Update(*world, false);
		
		renderer->Render(*world, 0, 0, entry.framebuffer.handle, LEVEL_THUMBNAIL_RES_X, LEVEL_THUMBNAIL_RES_Y);
		
		entry.downloadBuffer = eg::Buffer(
			eg::BufferFlags::MapRead | eg::BufferFlags::CopyDst | eg::BufferFlags::Download | eg::BufferFlags::HostAllocate,
			THUMBNAIL_BYTES, nullptr);
		
		eg::TextureRange range = {};
		range.sizeX = LEVEL_THUMBNAIL_RES_X;
		range.sizeY = LEVEL_THUMBNAIL_RES_Y;
		range.sizeZ = 1;
		eg::DC.GetTextureData(entry.texture, range, entry.downloadBuffer, 0);
		
		numUpdated++;
	}
	
	std::string message = "Updated " + std::to_string(numUpdated) + " thumbnails";
	writer.WriteLine(eg::console::InfoColor, message);
	
	eg::Profiler::current = profiler;
	
	return update;
}

void EndUpdateLevelThumbnails(LevelThumbnailUpdate* update)
{
	if (update == nullptr)
		return;
	
	char flippedData[THUMBNAIL_BYTES];
	
	for (LevelThumbnailEntry& thumbnail : update->thumbnails)
	{
		std::ofstream thumbnailStream(thumbnail.thumbnailPath, std::ios::binary);
		if (!thumbnailStream)
		{
			std::string errorMessage = "Error opening file for writing: " + thumbnail.thumbnailPath;
			eg::console::Write(eg::console::ErrorColor, errorMessage);
			continue;
		}
		
		thumbnail.downloadBuffer.Invalidate(0, THUMBNAIL_BYTES);
		char* data = static_cast<char*>(thumbnail.downloadBuffer.Map(0, THUMBNAIL_BYTES));
		if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL)
		{
			for (uint32_t y = 0; y < LEVEL_THUMBNAIL_RES_Y; y++)
			{
				uint32_t srcRowOffset = y * LEVEL_THUMBNAIL_RES_X * 4;
				uint32_t dstRowOffset = (LEVEL_THUMBNAIL_RES_Y - y - 1) * LEVEL_THUMBNAIL_RES_X * 4;
				std::memcpy(flippedData + dstRowOffset, data + srcRowOffset, LEVEL_THUMBNAIL_RES_X * 4);
			}
			data = flippedData;
		}
		
		if (!eg::WriteImageToStream(thumbnailStream, eg::WriteImageFormat::JPG,
			LEVEL_THUMBNAIL_RES_X, LEVEL_THUMBNAIL_RES_Y, 4, {data, THUMBNAIL_BYTES}))
		{
			continue;
		}
		thumbnailStream.close();
		
		LoadLevelThumbnail(*thumbnail.level);
	}
	
	delete update;
}
