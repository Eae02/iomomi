#include "GuiCommon.hpp"

#include "../AudioPlayers.hpp"

#include <glm/gtx/matrix_transform_2d.hpp>

const eg::ColorLin style::ButtonColorDefault(eg::ColorSRGB::FromRGBAHex(0x516a76B0));
const eg::ColorLin style::ButtonColorHover(eg::ColorSRGB::FromRGBAHex(0x33a8e6c0));

const eg::SpriteFont* style::UIFont;
const eg::SpriteFont* style::UIFontSmall;

static const eg::AudioClip* menuMouseHoverSound;

static void OnInit()
{
	style::UIFont = &eg::GetAsset<eg::SpriteFont>("UIFont.ttf");
	style::UIFontSmall = &eg::GetAsset<eg::SpriteFont>("UIFontSmall.ttf");
	menuMouseHoverSound = &eg::GetAsset<eg::AudioClip>("Audio/MenuMouseOver.ogg");
}

EG_ON_INIT(OnInit)

void PlayButtonHoverSound()
{
	AudioPlayers::menuSFXPlayer.Play(*menuMouseHoverSound, BUTTON_HOVER_VOLUME, 1, nullptr);
}

GuiFrameArgs GuiFrameArgs::MakeForCurrentFrame(float dt)
{
	constexpr float CANVAS_SIZE_SMALLER_DIMENSION = 1000.0f;

	float canvasScale = CANVAS_SIZE_SMALLER_DIMENSION /
	                    static_cast<float>(std::min(eg::CurrentResolutionY(), eg::CurrentResolutionX()));

	return GuiFrameArgs{
		.dt = dt,
		.cursorPos =
			glm::vec2(eg::CursorX(), static_cast<float>(eg::CurrentResolutionY()) - eg::CursorY() - 1) * canvasScale,
		.scaleToCanvasCoordinates = canvasScale,
		.scaleToScreenCoordinates = 1.0f / canvasScale,
		.canvasWidth = eg::CurrentResolutionX() * canvasScale,
		.canvasHeight = eg::CurrentResolutionY() * canvasScale,
	};
}

glm::mat3 GuiFrameArgs::GetMatrixToNDC() const
{
	return glm::translate(glm::mat3(1), glm::vec2(-1)) *
	       glm::scale(glm::mat3(1), glm::vec2(2.0f / canvasWidth, 2.0f / canvasHeight));
}
