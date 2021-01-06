#include "GuiCommon.hpp"

const eg::ColorLin style::ButtonColorDefault(eg::ColorSRGB::FromRGBAHex(0x516a76B0));
const eg::ColorLin style::ButtonColorHover(eg::ColorSRGB::FromRGBAHex(0x33a8e6c0));

const eg::SpriteFont* style::UIFont;
const eg::SpriteFont* style::UIFontSmall;

static void OnInit()
{
	style::UIFont = &eg::GetAsset<eg::SpriteFont>("UIFont.ttf");
	style::UIFontSmall = &eg::GetAsset<eg::SpriteFont>("UIFontSmall.ttf");
}

EG_ON_INIT(OnInit)
