//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp (LinearizeColors).
// Header-only: Dear ImGui produces sRGB vertex colors; render targets that
// store linear values (sRGB and floating-point formats) need them converted.
//

#ifndef HACKFRAMEWORK_IMGUICOLORS_H
#define HACKFRAMEWORK_IMGUICOLORS_H

#include <array>
#include <cmath>

#include <imgui.h>

namespace ImGuiColors
{

    inline void LinearizeColors(ImDrawData* drawData)
    {
        static const std::array<ImU32, 256> kLinear = [] {
            std::array<ImU32, 256> table{};
            for (int i = 0; i < 256; ++i)
            {
                float c = i / 255.0f;
                float linear = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
                table[i] = static_cast<ImU32>(linear * 255.0f + 0.5f);
            }
            return table;
        }();
        for (ImDrawList* list : drawData->CmdLists)
        {
            for (ImDrawVert& vertex : list->VtxBuffer)
            {
                ImU32 c = vertex.col;
                vertex.col = (c & IM_COL32_A_MASK) |
                             kLinear[(c >> IM_COL32_R_SHIFT) & 0xFF] << IM_COL32_R_SHIFT |
                             kLinear[(c >> IM_COL32_G_SHIFT) & 0xFF] << IM_COL32_G_SHIFT |
                             kLinear[(c >> IM_COL32_B_SHIFT) & 0xFF] << IM_COL32_B_SHIFT;
            }
        }
    }

} // namespace ImGuiColors

#endif // HACKFRAMEWORK_IMGUICOLORS_H
