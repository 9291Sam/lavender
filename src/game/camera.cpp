#include "camera.hpp"
#include "game.hpp"
#include "game/transform.hpp"
#include <format>
#include <gfx/renderer.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <util/log.hpp>

namespace game
{
    Camera::Camera()
        : Camera {glm::vec3 {0.0, 0.0, 0.0}}
    {}

    Camera::Camera(glm::vec3 position)
        : pitch {0.0f}
        , yaw {0.0f} // clang-format off
        , transform {
            .rotation {1.0f, 0.0f, 0.0f, 0.0f},
            .translation {position},
            .scale {1.0f, 1.0f, 1.0f}
            }
    // clang-format on
    {}

    glm::mat4 InfiniteReversedPerspective(float fov, float aspectRatio, float nearPlane)
    {
        float f = 1.0f / tan(fov * 0.5f);

        glm::mat4 result = glm::mat4(0.0f);
        result[0][0]     = f / aspectRatio;
        result[1][1]     = f;
        result[2][2]     = 0.0f; // This makes the depth buffer reversed
        result[2][3]     = -1.0f;
        result[3][2]     = nearPlane; // Pushing near plane to zero
        result[3][3]     = 0.0f;

        return result;
    }

    glm::mat4 Camera::getPerspectiveMatrix(const Game& game, const Transform& transform_) const
    {
        const glm::mat4 projection =
            InfiniteReversedPerspective(game.getFovYRadians(), game.getAspectRatio(), 0.1f);
        // glm::infinitePerspective(game.getFovYRadians(), game.getAspectRatio(), 5.0f);

        return projection * this->getViewMatrix() * transform_.asModelMatrix();
    }

    glm::mat4 Camera::getModelMatrix() const
    {
        return this->transform.asModelMatrix();
    }

    glm::mat4 Camera::getViewMatrix() const
    {
        return glm::inverse(
            this->transform.asTranslationMatrix() * this->transform.asRotationMatrix());
    }

    glm::vec3 Camera::getForwardVector() const
    {
        return this->transform.getForwardVector();
    }

    glm::vec3 Camera::getRightVector() const
    {
        return this->transform.getRightVector();
    }

    glm::vec3 Camera::getUpVector() const
    {
        return this->transform.getUpVector();
    }

    glm::vec3 Camera::getPosition() const
    {
        return this->transform.translation;
    }

    float Camera::getPitch() const
    {
        return this->pitch;
    }

    float Camera::getYaw() const
    {
        return this->yaw;
    }

    game::Transform Camera::getTransform() const
    {
        return this->transform;
    }

    void Camera::setPosition(glm::vec3 newPos)
    {
        this->transform.translation = newPos;
    }

    void Camera::addPosition(glm::vec3 positionToAdd)
    {
        this->transform.translation += positionToAdd;
    }

    void Camera::addPitch(float pitchToAdd)
    {
        this->pitch += pitchToAdd;
        this->updateTransformFromRotations();
        // const float pitchBefore =
        // glm::eulerAngles(this->transform.rotation).p; if (pitchBefore +
        // pitchToAdd > glm::half_pi<float>())
        // {
        //     this->transform.pitchBy(glm::half_pi<float>() - pitchBefore);

        //     util::logTrace("Yawing by {:.8f}", yawToAdd);
        // }
        // else
        // {
        //     this->transform.pitchBy(pitchToAdd);
        // }
    }

    void Camera::addYaw(float yawToAdd)
    {
        this->yaw += yawToAdd;
        this->updateTransformFromRotations();
    }

    Camera::operator std::string () const
    {
        return std::format(
            "{}Pitch {} | Yaw {}",
            static_cast<std::string>(this->transform),
            this->pitch,
            this->yaw);
    }

    void Camera::updateTransformFromRotations()
    {
        glm::quat q {1.0f, 0.0f, 0.0f, 0.0f};

        this->pitch = std::clamp(this->pitch, -glm::half_pi<float>(), glm::half_pi<float>());
        this->yaw   = glm::mod(this->yaw, glm::two_pi<float>());

        q *= glm::angleAxis(this->pitch, -Transform::RightVector);

        q = glm::angleAxis(this->yaw, -Transform::UpVector) * q;

        this->transform.rotation = q;
    }
} // namespace game
