#pragma once

#include <memory>

namespace game
{
    namespace frame
    {
        class FrameGenerator;
    } // namespace frame

    class Game
    {
    public:
        Game();
        ~Game() noexcept;

        Game(const Game&)             = delete;
        Game(Game&&)                  = delete;
        Game& operator= (const Game&) = delete;
        Game& operator= (Game&&)      = delete;

        void run();


    private:
        // lowest level is gfx
        std::unique_ptr<frame::FrameGenerator> frame_generator;
        // game renderer
    };
} // namespace game
