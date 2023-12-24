#include "GuiCommon.hpp"

#include "../AudioPlayers.hpp"

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
