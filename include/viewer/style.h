#pragma once

#include "imgui.h"

namespace tattler
{

enum class Theme { RosePineDawn, RosePine };

// All colours for a single Rose Pine variant
struct Palette
{
    ImVec4 base, surface, overlay;
    ImVec4 muted, subtle, text;
    ImVec4 love, gold, rose, pine, foam, iris;
    ImVec4 hlLow, hlMed, hlHigh;
};

// Rose Pine Dawn (light) — https://rosepinetheme.com/palette/ingredients/
inline constexpr Palette ROSE_PINE_DAWN = {
    {0.980f, 0.957f, 0.929f, 1.000f}, // base    #faf4ed
    {1.000f, 0.980f, 0.953f, 1.000f}, // surface #fffaf3
    {0.949f, 0.914f, 0.882f, 1.000f}, // overlay #f2e9e1
    {0.596f, 0.576f, 0.647f, 1.000f}, // muted   #9893a5
    {0.475f, 0.459f, 0.576f, 1.000f}, // subtle  #797593
    {0.341f, 0.322f, 0.475f, 1.000f}, // text    #575279
    {0.706f, 0.388f, 0.478f, 1.000f}, // love    #b4637a
    {0.918f, 0.616f, 0.204f, 1.000f}, // gold    #ea9d34
    {0.843f, 0.510f, 0.494f, 1.000f}, // rose    #d7827e
    {0.157f, 0.412f, 0.514f, 1.000f}, // pine    #286983
    {0.337f, 0.580f, 0.624f, 1.000f}, // foam    #56949f
    {0.565f, 0.478f, 0.663f, 1.000f}, // iris    #907aa9
    {0.957f, 0.929f, 0.910f, 1.000f}, // hlLow   #f4ede8
    {0.875f, 0.855f, 0.851f, 1.000f}, // hlMed   #dfdad9
    {0.808f, 0.792f, 0.804f, 1.000f}, // hlHigh  #cecacd
};

// Rose Pine (dark) — https://rosepinetheme.com/palette/ingredients/
inline constexpr Palette ROSE_PINE = {
    {0.098f, 0.090f, 0.141f, 1.000f}, // base    #191724
    {0.122f, 0.114f, 0.180f, 1.000f}, // surface #1f1d2e
    {0.149f, 0.137f, 0.227f, 1.000f}, // overlay #26233a
    {0.431f, 0.416f, 0.525f, 1.000f}, // muted   #6e6a86
    {0.565f, 0.549f, 0.667f, 1.000f}, // subtle  #908caa
    {0.878f, 0.871f, 0.957f, 1.000f}, // text    #e0def4
    {0.922f, 0.435f, 0.573f, 1.000f}, // love    #eb6f92
    {0.965f, 0.757f, 0.467f, 1.000f}, // gold    #f6c177
    {0.922f, 0.737f, 0.729f, 1.000f}, // rose    #ebbcba
    {0.192f, 0.455f, 0.561f, 1.000f}, // pine    #31748f
    {0.612f, 0.812f, 0.847f, 1.000f}, // foam    #9ccfd8
    {0.769f, 0.655f, 0.906f, 1.000f}, // iris    #c4a7e7
    {0.129f, 0.125f, 0.180f, 1.000f}, // hlLow   #21202e
    {0.251f, 0.239f, 0.322f, 1.000f}, // hlMed   #403d52
    {0.322f, 0.310f, 0.404f, 1.000f}, // hlHigh  #524f67
};

/// <summary> Returns the palette for the currently active theme. </summary>
auto GetCurrentPalette() -> const Palette&;

/// <summary> Apply a Rose Pine theme and rounded style to the active ImGui context. </summary>
auto ApplyTheme(Theme theme) -> void;

} // namespace tattler
