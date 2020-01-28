#include "GuiCommon.hpp"

const eg::ColorLin style::ButtonColorDefault(eg::ColorSRGB::FromRGBAHex(0x63696c90));
const eg::ColorLin style::ButtonColorHover(eg::ColorSRGB::FromRGBAHex(0x33a8e6c0));

const eg::SpriteFont* style::UIFont;

static void OnInit()
{
	style::UIFont = &eg::GetAsset<eg::SpriteFont>("UIFont.ttf");
}

EG_ON_INIT(OnInit)
