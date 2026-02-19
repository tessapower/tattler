#include "stdafx.h"

#include "viewer/style.h"

#include "imgui.h"

namespace tattler
{

static Theme s_currentTheme = Theme::RosePineDawn;

auto GetCurrentPalette() -> const Palette&
{
    return s_currentTheme == Theme::RosePineDawn ? ROSE_PINE_DAWN : ROSE_PINE;
}

static void ApplyPalette(const Palette& p)
{
    ImVec4* c = ImGui::GetStyle().Colors;
    c[ImGuiCol_Text]                      = p.text;
    c[ImGuiCol_TextDisabled]              = p.muted;
    c[ImGuiCol_WindowBg]                  = p.base;
    c[ImGuiCol_ChildBg]                   = p.base;
    c[ImGuiCol_PopupBg]                   = p.surface;
    c[ImGuiCol_Border]                    = p.hlMed;
    c[ImGuiCol_BorderShadow]              = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg]                   = p.overlay;
    c[ImGuiCol_FrameBgHovered]            = p.hlMed;
    c[ImGuiCol_FrameBgActive]             = p.hlHigh;
    c[ImGuiCol_TitleBg]                   = p.surface;
    c[ImGuiCol_TitleBgActive]             = p.overlay;
    c[ImGuiCol_TitleBgCollapsed]          = p.surface;
    c[ImGuiCol_MenuBarBg]                 = p.surface;
    c[ImGuiCol_ScrollbarBg]               = p.base;
    c[ImGuiCol_ScrollbarGrab]             = p.hlHigh;
    c[ImGuiCol_ScrollbarGrabHovered]      = p.muted;
    c[ImGuiCol_ScrollbarGrabActive]       = p.subtle;
    c[ImGuiCol_CheckMark]                 = p.pine;
    c[ImGuiCol_SliderGrab]                = p.foam;
    c[ImGuiCol_SliderGrabActive]          = p.pine;
    c[ImGuiCol_Button]                    = p.overlay;
    c[ImGuiCol_ButtonHovered]             = p.hlMed;
    c[ImGuiCol_ButtonActive]              = p.hlHigh;
    c[ImGuiCol_Header]                    = p.hlLow;
    c[ImGuiCol_HeaderHovered]             = p.hlMed;
    c[ImGuiCol_HeaderActive]              = p.hlHigh;
    c[ImGuiCol_Separator]                 = p.hlMed;
    c[ImGuiCol_SeparatorHovered]          = p.muted;
    c[ImGuiCol_SeparatorActive]           = p.subtle;
    c[ImGuiCol_ResizeGrip]                = {p.pine.x, p.pine.y, p.pine.z, 0.2f};
    c[ImGuiCol_ResizeGripHovered]         = {p.pine.x, p.pine.y, p.pine.z, 0.5f};
    c[ImGuiCol_ResizeGripActive]          = p.pine;
    c[ImGuiCol_TabHovered]                = p.overlay;
    c[ImGuiCol_Tab]                       = p.surface;
    c[ImGuiCol_TabSelected]               = p.hlLow;
    c[ImGuiCol_TabSelectedOverline]       = p.pine;
    c[ImGuiCol_TabDimmed]                 = p.surface;
    c[ImGuiCol_TabDimmedSelected]         = p.overlay;
    c[ImGuiCol_TabDimmedSelectedOverline] = p.muted;
    c[ImGuiCol_DockingPreview]            = {p.pine.x, p.pine.y, p.pine.z, 0.4f};
    c[ImGuiCol_DockingEmptyBg]            = p.base;
    c[ImGuiCol_PlotLines]                 = p.foam;
    c[ImGuiCol_PlotLinesHovered]          = p.pine;
    c[ImGuiCol_PlotHistogram]             = p.gold;
    c[ImGuiCol_PlotHistogramHovered]      = p.foam;
    c[ImGuiCol_TableHeaderBg]             = p.overlay;
    c[ImGuiCol_TableBorderStrong]         = p.hlHigh;
    c[ImGuiCol_TableBorderLight]          = p.hlMed;
    c[ImGuiCol_TableRowBg]                = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]             = {p.hlLow.x, p.hlLow.y, p.hlLow.z, 0.5f};
    c[ImGuiCol_TextLink]                  = p.pine;
    c[ImGuiCol_TextSelectedBg]            = {p.pine.x, p.pine.y, p.pine.z, 0.3f};
    c[ImGuiCol_DragDropTarget]            = p.gold;
    c[ImGuiCol_NavCursor]                 = p.pine;
    c[ImGuiCol_NavWindowingHighlight]     = {p.pine.x, p.pine.y, p.pine.z, 0.7f};
    c[ImGuiCol_NavWindowingDimBg]         = {p.base.x, p.base.y, p.base.z, 0.8f};
    c[ImGuiCol_ModalWindowDimBg]          = {p.base.x, p.base.y, p.base.z, 0.6f};

    // Rounded, clean style
    ImGuiStyle& style       = ImGui::GetStyle();
    style.FramePadding      = {6.0f, 5.0f}; // +2px over ImGui default {4,3}
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
}

auto ApplyTheme(Theme theme) -> void
{
    s_currentTheme = theme;
    ApplyPalette(theme == Theme::RosePineDawn ? ROSE_PINE_DAWN : ROSE_PINE);
}

} // namespace tattler
