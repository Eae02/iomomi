#pragma once

constexpr eg::Format EDITOR_COLOR_FORMAT = eg::Format::R8G8B8A8_sRGB;
constexpr eg::Format EDITOR_DEPTH_FORMAT = eg::Format::Depth32;

constexpr eg::ColorAndDepthFormat EDITOR_FB_FORMAT(EDITOR_COLOR_FORMAT, EDITOR_DEPTH_FORMAT);
