#pragma once

#include "game/frame_generator.hpp"

namespace game
{
    class Game;
} // namespace game

namespace verdigris
{
    class SkyboxManager
    {
    public:
        explicit SkyboxManager(const game::Game*);
        ~SkyboxManager() = default;

        SkyboxManager(const SkyboxManager&)             = delete;
        SkyboxManager(SkyboxManager&&)                  = delete;
        SkyboxManager& operator= (const SkyboxManager&) = delete;
        SkyboxManager& operator= (SkyboxManager&&)      = delete;

        [[nodiscard]] game::FrameGenerator::RecordObject getRecordObject() const;

    private:
        std::shared_ptr<vk::UniquePipeline> pipeline;
        const game::Game*                   game;
    };
}; // namespace verdigris