#pragma once

#include "imgui.h"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <glm/gtx/string_cast.hpp>
#include <glm/vec2.hpp>
#include <map>
#include <span>
#include <vector>

// Heavily adapted from https://github.com/Raikiri/LegitProfiler

namespace gfx::profiler
{
    // https://flatuicolors.com/palette/defo
    constexpr u32 convertToColor(u32 col) noexcept
    {
        return (
            ((col & 0xff000000U) >> (3 * 8U)) + ((col & 0x00ff0000U) >> (1 * 8U))
            + ((col & 0x0000ff00U) << (1 * 8U)) + ((col & 0x000000ffU) << (3 * 8U)));
    }
    static constexpr u32 Turquoise   = convertToColor(0x1abc9cffu);
    static constexpr u32 GreenSea    = convertToColor(0x16a085ffu);
    static constexpr u32 Emerald     = convertToColor(0x2ecc71ffu);
    static constexpr u32 Nephritis   = convertToColor(0x27ae60ffu);
    static constexpr u32 PeterRiver  = convertToColor(0x3498dbffu);
    static constexpr u32 BelizeHole  = convertToColor(0x2980b9ffu);
    static constexpr u32 Amethyst    = convertToColor(0x9b59b6ffu);
    static constexpr u32 Wisteria    = convertToColor(0x8e44adffu);
    static constexpr u32 SunFlower   = convertToColor(0xf1c40fffu);
    static constexpr u32 Orange      = convertToColor(0xf39c12ffu);
    static constexpr u32 Carrot      = convertToColor(0xe67e22ffu);
    static constexpr u32 Pumpkin     = convertToColor(0xd35400ffu);
    static constexpr u32 Alizarin    = convertToColor(0xe74c3cffu);
    static constexpr u32 Pomegranate = convertToColor(0xc0392bffu);
    static constexpr u32 Clouds      = convertToColor(0xecf0f1ffu);
    static constexpr u32 Silver      = convertToColor(0xbdc3c7ffu);
    static constexpr u32 ImguiText   = Silver;

    static constexpr std::array<u32, 16> Colors = {
        Turquoise,
        Emerald,
        PeterRiver,
        Amethyst,
        SunFlower,
        Carrot,
        Alizarin,
        Clouds,
        GreenSea,
        Nephritis,
        BelizeHole,
        Wisteria,
        Orange,
        Pumpkin,
        Pomegranate,
        Silver};

    struct ProfilerTask
    {
        float       start_time = 0.0f;
        float       end_time   = 0.0f;
        std::string name       = "Unnamed";
        u32         color      = Carrot;

        [[nodiscard]] float getLength() const
        {
            return this->end_time - this->start_time;
        }
    };

    class ProfilerGraph
    {
    public:
        int   frame_width;
        int   frame_spacing;
        bool  use_colored_legend_text;
        float max_frame_time = 1.0f / 30.0f;

        explicit ProfilerGraph(size_t framesCount)
            : frame_width {1}
            , frame_spacing {0}
            , use_colored_legend_text {true}
        {
            frames.resize(framesCount);
            for (auto& frame : frames)
            {
                frame.tasks.reserve(100);
            }
        }

        void loadFrameData(std::span<const ProfilerTask> tasks)
        {
            auto& currFrame = frames[current_frame_index];
            currFrame.tasks.resize(0);

            for (size_t taskIndex = 0; taskIndex < tasks.size(); taskIndex++)
            {
                if (taskIndex == 0)
                {
                    currFrame.tasks.push_back(tasks[taskIndex]);
                }
                else
                {
                    if (tasks[taskIndex - 1].color != tasks[taskIndex].color
                        || tasks[taskIndex - 1].name != tasks[taskIndex].name)
                    {
                        currFrame.tasks.push_back(tasks[taskIndex]);
                    }
                    else
                    {
                        currFrame.tasks.back().end_time = tasks[taskIndex].end_time;
                    }
                }
            }
            currFrame.task_stats_index.resize(currFrame.tasks.size());

            for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
            {
                auto& task = currFrame.tasks[taskIndex];
                auto  it   = task_name_to_stats_index.find(task.name);
                if (it == task_name_to_stats_index.end())
                {
                    task_name_to_stats_index[task.name] = task_stats.size();
                    TaskStats taskStat {};
                    taskStat.max_time = -1.0;
                    task_stats.push_back(taskStat);
                }
                currFrame.task_stats_index[taskIndex] = task_name_to_stats_index[task.name];
            }
            current_frame_index = (current_frame_index + 1) % frames.size();

            rebuildTaskStats(current_frame_index, this->frames.size());
        }

        void renderTimings(
            std::size_t graphWidth,
            std::size_t legendWidth,
            std::size_t height,
            std::size_t frameIndexOffset)
        {
            ImDrawList*     drawList  = ImGui::GetWindowDrawList();
            const ImVec2    screenPos = ImGui::GetCursorScreenPos();
            const glm::vec2 widgetPos {screenPos.x, screenPos.y};

            this->renderGraph(
                drawList, widgetPos, glm::vec2(graphWidth - 5, height), frameIndexOffset);

            this->renderLegend(
                drawList,
                widgetPos,
                glm::vec2(legendWidth, height + (16 * this->task_stats.size())), // Font spacing
                frameIndexOffset);

            ImGui::Dummy(ImVec2(
                static_cast<float>(graphWidth + legendWidth),
                static_cast<float>(height + (16 * this->task_stats.size()))));
        }

    private:
        void rebuildTaskStats(size_t endFrame, size_t framesCount) // NOLINT
        {
            for (auto& taskStat : task_stats)
            {
                taskStat.max_time        = -1.0f;
                taskStat.priority_order  = static_cast<size_t>(-1);
                taskStat.on_screen_index = static_cast<size_t>(-1);
            }

            for (size_t frameNumber = 0; frameNumber < framesCount; frameNumber++)
            {
                size_t frameIndex = (endFrame - 1 - frameNumber + frames.size()) % frames.size();
                auto&  frame      = frames[frameIndex];
                for (size_t taskIndex = 0; taskIndex < frame.tasks.size(); taskIndex++)
                {
                    auto& task     = frame.tasks[taskIndex];
                    auto& stats    = task_stats[frame.task_stats_index[taskIndex]];
                    stats.max_time = std::max(stats.max_time, task.end_time - task.start_time);
                }
            }
            std::vector<size_t> statPriorities;
            statPriorities.resize(task_stats.size());
            for (size_t statIndex = 0; statIndex < task_stats.size(); statIndex++)
            {
                statPriorities[statIndex] = statIndex;
            }

            std::ranges::sort(
                statPriorities,
                [this](size_t left, size_t right)
                {
                    return task_stats[left].max_time > task_stats[right].max_time;
                });
            for (size_t statNumber = 0; statNumber < task_stats.size(); statNumber++)
            {
                size_t statIndex                     = statPriorities[statNumber];
                task_stats[statIndex].priority_order = statNumber;
            }
        }
        void renderGraph(
            ImDrawList* drawList, glm::vec2 graphPos, glm::vec2 graphSize, size_t frameIndexOffset)
        {
            rect(drawList, graphPos, graphPos + graphSize, 0xffffffff, false);
            float heightThreshold = 1.0f;

            const glm::vec2 bottomLeftCorner = graphPos + glm::vec2 {0.0f, graphSize.y};

            auto drawMsLine = [&](float time, u32 color)
            {
                const float height10ms = ((time) / max_frame_time) * graphSize.y;
                rect(
                    drawList,
                    bottomLeftCorner + glm::vec2(0.0f, -height10ms),
                    bottomLeftCorner + glm::vec2(graphSize.x, -height10ms - 1.0f),
                    color);

                std::array<char, 64> buffer {};
                std::format_to_n(buffer.begin(), 64, "{}", time);

                text(
                    drawList,
                    bottomLeftCorner + glm::vec2(2.0f, -height10ms - 16),
                    color,
                    buffer.data());
            };

            drawMsLine(0.005f, convertToColor(0x256D7BFF));
            drawMsLine(0.010f, convertToColor(0xC7B446FF));
            drawMsLine(0.016f, convertToColor(0xCB3234FF));
            drawMsLine(0.022f, convertToColor(0x85219CFF));

            for (size_t frameNumber = 0; frameNumber < frames.size(); frameNumber++)
            {
                size_t frameIndex =
                    (current_frame_index - frameIndexOffset - 1 - frameNumber + 2 * frames.size())
                    % frames.size();

                glm::vec2 framePos =
                    graphPos
                    + glm::vec2(
                        graphSize.x - 1.0f - static_cast<float>(frame_width)
                            - static_cast<float>(
                                (frame_width + frame_spacing) * static_cast<int>(frameNumber)),
                        graphSize.y - 1);
                if (framePos.x < graphPos.x + 1)
                {
                    break;
                }
                glm::vec2 taskPos = framePos + glm::vec2(0.0f, 0.0f);
                auto&     frame   = frames[frameIndex];
                for (const auto& task : frame.tasks)
                {
                    float taskStartHeight = ((task.start_time) / max_frame_time) * graphSize.y;
                    float taskEndHeight   = ((task.end_time) / max_frame_time) * graphSize.y;
                    // taskMaxCosts[task.name] = std::max(taskMaxCosts[task.name], task.end_time -
                    // task.start_time);
                    if (abs(taskEndHeight - taskStartHeight) > heightThreshold)
                    {
                        rect(
                            drawList,
                            taskPos + glm::vec2(0.0f, -taskStartHeight),
                            taskPos + glm::vec2(frame_width, -taskEndHeight),
                            task.color,
                            true);
                    }
                }
            }
        }
        void renderLegend(
            ImDrawList* drawList,
            glm::vec2   legendPos,
            glm::vec2   legendSize,
            size_t      frameIndexOffset)
        {
            const float markerLeftRectMargin   = 3.0f;
            const float markerLeftRectWidth    = 5.0f;
            const float markerMidWidth         = 30.0f;
            const float markerRightRectWidth   = 10.0f;
            const float markerRightRectMargin  = 3.0f;
            const float markerRightRectHeight  = 10.0f;
            const float markerRightRectSpacing = 4.0f;

            auto& currFrame = frames
                [(current_frame_index - frameIndexOffset - 1 + 2 * frames.size()) % frames.size()];
            size_t maxTasksCount = static_cast<size_t>(
                legendSize.y / (markerRightRectHeight + markerRightRectSpacing));

            for (auto& taskStat : task_stats)
            {
                taskStat.on_screen_index = static_cast<size_t>(-1);
            }

            size_t tasksToShow     = std::min<size_t>(task_stats.size(), maxTasksCount);
            size_t tasksShownCount = 0;
            for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
            {
                auto& task = currFrame.tasks[taskIndex];
                auto& stat = task_stats[currFrame.task_stats_index[taskIndex]];

                if (stat.priority_order >= tasksToShow)
                {
                    continue;
                }

                if (stat.on_screen_index == static_cast<size_t>(-1))
                {
                    stat.on_screen_index = tasksShownCount++;
                }
                else
                {
                    continue;
                }
                float taskStartHeight = (task.start_time / max_frame_time) * legendSize.y;
                float taskEndHeight   = (task.end_time / max_frame_time) * legendSize.y;

                glm::vec2 markerLeftRectMin =
                    legendPos + glm::vec2(markerLeftRectMargin, legendSize.y);
                glm::vec2 markerLeftRectMax =
                    markerLeftRectMin + glm::vec2(markerLeftRectWidth, 0.0f);
                markerLeftRectMin.y -= taskStartHeight;
                markerLeftRectMax.y -= taskEndHeight;

                glm::vec2 markerRightRectMin =
                    legendPos
                    + glm::vec2(
                        markerLeftRectMargin + markerLeftRectWidth + markerMidWidth,
                        legendSize.y - markerRightRectMargin
                            - ((markerRightRectHeight + markerRightRectSpacing)
                               * static_cast<float>(stat.on_screen_index)));

                glm::vec2 markerRightRectMax =
                    markerRightRectMin + glm::vec2(markerRightRectWidth, -markerRightRectHeight);

                u32 textColor =
                    use_colored_legend_text ? task.color : gfx::profiler::ImguiText; // task.color;

                float              taskTimeMs = (task.end_time - task.start_time);
                std::ostringstream timeText;
                timeText.precision(2);
                timeText << std::fixed << "[" << (taskTimeMs * 1000.0f) << "ms] " << task.name;

                text(
                    drawList,
                    glm::vec2 {markerLeftRectMin.x - 5, markerRightRectMax.y - 5},
                    textColor,
                    timeText.str().c_str());
            }
        }

        static void rect(
            ImDrawList* drawList,
            glm::vec2   minPoint,
            glm::vec2   maxPoint,
            u32         col,
            bool        filled = true)
        {
            if (filled)
            {
                drawList->AddRectFilled(
                    ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
            }
            else
            {
                drawList->AddRect(
                    ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
            }
        }

        static void text(ImDrawList* drawList, glm::vec2 point, u32 col, const char* text)
        {
            drawList->AddText(ImVec2(point.x, point.y), col, text);
        }
        static void
        triangle(ImDrawList* drawList, std::array<glm::vec2, 3> points, u32 col, bool filled = true)
        {
            if (filled)
            {
                drawList->AddTriangleFilled(
                    ImVec2(points[0].x, points[0].y),
                    ImVec2(points[1].x, points[1].y),
                    ImVec2(points[2].x, points[2].y),
                    col);
            }
            else
            {
                drawList->AddTriangle(
                    ImVec2(points[0].x, points[0].y),
                    ImVec2(points[1].x, points[1].y),
                    ImVec2(points[2].x, points[2].y),
                    col);
            }
        }
        static void renderTaskMarker(
            ImDrawList* drawList,
            glm::vec2   leftMinPoint,
            glm::vec2   leftMaxPoint,
            glm::vec2   rightMinPoint,
            glm::vec2   rightMaxPoint,
            u32         col)
        {
            rect(drawList, leftMinPoint, leftMaxPoint, col, true);
            rect(drawList, rightMinPoint, rightMaxPoint, col, true);
            std::array<ImVec2, 4> points = {
                ImVec2(leftMaxPoint.x, leftMinPoint.y),
                ImVec2(leftMaxPoint.x, leftMaxPoint.y),
                ImVec2(rightMinPoint.x, rightMaxPoint.y),
                ImVec2(rightMinPoint.x, rightMinPoint.y)};
            drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), col);
        }
        struct FrameData
        {
            std::vector<profiler::ProfilerTask> tasks;
            std::vector<size_t>                 task_stats_index;
        };

        struct TaskStats
        {
            float  max_time;
            size_t priority_order;
            size_t on_screen_index;
        };
        std::vector<TaskStats>        task_stats;
        std::map<std::string, size_t> task_name_to_stats_index;

        std::vector<FrameData> frames;
        size_t                 current_frame_index = 0;
    };
} // namespace gfx::profiler
