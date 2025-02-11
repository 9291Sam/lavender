#pragma once

#include "game/transform.hpp"
#include "transform.hpp"

namespace game
{
    class Game;

    class Camera
    {
    public:

        Camera();
        explicit Camera(glm::vec3 position);

        [[nodiscard]] glm::mat4 getPerspectiveMatrix(const Game&, const Transform&) const;
        [[nodiscard]] glm::mat4 getModelMatrix() const;
        [[nodiscard]] glm::mat4 getViewMatrix() const;
        [[nodiscard]] glm::vec3 getForwardVector() const;
        [[nodiscard]] glm::vec3 getRightVector() const;
        [[nodiscard]] glm::vec3 getUpVector() const;
        [[nodiscard]] glm::vec3 getPosition() const;
        [[nodiscard]] float     getPitch() const;
        [[nodiscard]] float     getYaw() const;

        [[nodiscard]] game::Transform getTransform() const;

        void setPosition(glm::vec3);
        void addPosition(glm::vec3);
        void addPitch(float);
        void addYaw(float);

        explicit operator std::string () const;

    private:
        void updateTransformFromRotations();

        float pitch;
        float yaw;

        Transform transform;
    };
} // namespace game
