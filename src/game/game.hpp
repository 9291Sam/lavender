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


    private:
        std::unique_ptr<frame::FrameGenerator> frame_generator;
    };
} // namespace game
