#pragma once

#include <cstddef>
#include <gfx/vulkan/buffer.hpp>

namespace voxel::chunk
{
    class Chunk
    {
    public:
        static constexpr std::size_t EdgeLength = 32;
    public:

        Chunk();
        ~Chunk();

        Chunk(const Chunk&)             = delete;
        Chunk(Chunk&&)                  = delete;
        Chunk& operator= (const Chunk&) = delete;
        Chunk& operator= (Chunk&&)      = delete;

        // void write


    private:
        vk::DescriptorSet   positions_descriptor_set;
        gfx::vulkan::Buffer positions_buffer;
    };
} // namespace voxel::chunk