#pragma once

#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace game
{
    struct Transform
    {
        static constexpr glm::vec3 ForwardVector {0.0f, 0.0f, -1.0f};
        static constexpr glm::vec3 RightVector {1.0f, 0.0f, 0.0f};
        static constexpr glm::vec3 UpVector {0.0f, 1.0f, 0.0f};

        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 translation {0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};

        [[nodiscard]] glm::mat4 asModelMatrix() const;
        [[nodiscard]] glm::mat4 asTranslationMatrix() const;
        [[nodiscard]] glm::mat4 asRotationMatrix() const;
        [[nodiscard]] glm::mat4 asScaleMatrix() const;

        [[nodiscard]] glm::vec3 getForwardVector() const;
        [[nodiscard]] glm::vec3 getUpVector() const;
        [[nodiscard]] glm::vec3 getRightVector() const;

        void rotateBy(glm::quat);
        void pitchBy(float pitch);
        void yawBy(float yaw);
        void rollBy(float roll);

        explicit operator std::string () const;
    };

} // namespace game
