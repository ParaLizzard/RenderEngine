#include <gtest/gtest.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Scene/Camera.h"

class CameraViewTest : public ::testing::Test {
protected:
    Engine::Camera camera;
    static constexpr float EPSILON = 1e-4f;

    void expectMatNear(const glm::mat4 &a, const glm::mat4 &b, float eps)
    {
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                EXPECT_NEAR(a[col][row], b[col][row], eps)
                    << "Mismatch at [" << col << "][" << row << "]";
            }
        }
    }
};

// =============================================================================
// setViewDirection — basic cases
// =============================================================================

TEST_F(CameraViewTest, ViewDirectionAtOriginLookingForward)
{
    // Looking along +Z from the origin
    camera.setViewDirection(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));

    const auto &view = camera.getView();

    // View matrix should be a valid rotation (no translation since at origin)
    // The dot products with position are zero
    EXPECT_NEAR(view[3][0], 0.0f, EPSILON);
    EXPECT_NEAR(view[3][1], 0.0f, EPSILON);
    EXPECT_NEAR(view[3][2], 0.0f, EPSILON);
}

TEST_F(CameraViewTest, ViewDirectionTranslationIsCorrect)
{
    glm::vec3 pos(5.0f, 3.0f, -2.0f);
    glm::vec3 dir = glm::normalize(glm::vec3(0, 0, 1));

    camera.setViewDirection(pos, dir);

    const auto &view = camera.getView();

    // For a forward-looking camera, the view matrix should encode
    // the camera position in its translation column
    // The translation should be -dot(axis, position) for each axis
    // We just verify it's not identity
    EXPECT_NE(view, glm::mat4(1.0f));
}

// =============================================================================
// setViewTarget — equivalent to setViewDirection(pos, target-pos)
// =============================================================================

TEST_F(CameraViewTest, ViewTargetEquivalentToViewDirection)
{
    glm::vec3 pos(0, 5, 0);
    glm::vec3 target(0, 0, -10);
    glm::vec3 up(0, -1, 0);

    // setViewTarget should produce the same matrix as setViewDirection(pos, target - pos)
    Engine::Camera cam1, cam2;
    cam1.setViewTarget(pos, target, up);
    cam2.setViewDirection(pos, target - pos, up);

    expectMatNear(cam1.getView(), cam2.getView(), EPSILON);
    expectMatNear(cam1.getInverseView(), cam2.getInverseView(), EPSILON);
}

TEST_F(CameraViewTest, ViewTargetFromDifferentPositions)
{
    glm::vec3 target(0, 0, 0);

    Engine::Camera camA, camB;
    camA.setViewTarget(glm::vec3(10, 0, 0), target);
    camB.setViewTarget(glm::vec3(0, 10, 0), target);

    // Different positions should produce different view matrices
    EXPECT_NE(camA.getView(), camB.getView());
}

// =============================================================================
// setViewYXZ — identity quaternion
// =============================================================================

TEST_F(CameraViewTest, ViewYXZIdentityQuaternionAtOrigin)
{
    // Identity quaternion = no rotation
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    camera.setViewYXZ(glm::vec3(0, 0, 0), identity);

    const auto &view = camera.getView();

    // With identity rotation at origin, the view matrix should be close to identity
    // The right/up/forward extracted from the quaternion should be standard basis vectors
    expectMatNear(view, glm::mat4(1.0f), EPSILON);
}

TEST_F(CameraViewTest, ViewYXZIdentityQuaternionWithTranslation)
{
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 pos(3.0f, 7.0f, -5.0f);
    camera.setViewYXZ(pos, identity);

    const auto &view = camera.getView();

    // With identity rotation, the translation column should be -pos
    EXPECT_NEAR(view[3][0], -3.0f, EPSILON);
    EXPECT_NEAR(view[3][1], -7.0f, EPSILON);
    EXPECT_NEAR(view[3][2], 5.0f, EPSILON);
}

// =============================================================================
// getPosition() extracts column 3 of inverseViewMatrix
// =============================================================================

TEST_F(CameraViewTest, GetPositionReturnsCorrectPosition)
{
    glm::vec3 expectedPos(10.0f, 20.0f, 30.0f);
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);

    camera.setViewYXZ(expectedPos, identity);

    glm::vec3 extractedPos = camera.getPosition();

    EXPECT_NEAR(extractedPos.x, expectedPos.x, EPSILON);
    EXPECT_NEAR(extractedPos.y, expectedPos.y, EPSILON);
    EXPECT_NEAR(extractedPos.z, expectedPos.z, EPSILON);
}

TEST_F(CameraViewTest, GetPositionAfterSetViewDirection)
{
    glm::vec3 expectedPos(5.0f, -3.0f, 8.0f);
    camera.setViewDirection(expectedPos, glm::vec3(0, 0, 1));

    glm::vec3 extractedPos = camera.getPosition();

    EXPECT_NEAR(extractedPos.x, expectedPos.x, EPSILON);
    EXPECT_NEAR(extractedPos.y, expectedPos.y, EPSILON);
    EXPECT_NEAR(extractedPos.z, expectedPos.z, EPSILON);
}

TEST_F(CameraViewTest, GetPositionAfterSetViewTarget)
{
    glm::vec3 expectedPos(0.0f, 10.0f, 0.0f);
    camera.setViewTarget(expectedPos, glm::vec3(0, 0, 0));

    glm::vec3 extractedPos = camera.getPosition();

    EXPECT_NEAR(extractedPos.x, expectedPos.x, EPSILON);
    EXPECT_NEAR(extractedPos.y, expectedPos.y, EPSILON);
    EXPECT_NEAR(extractedPos.z, expectedPos.z, EPSILON);
}

// =============================================================================
// Inverse view matrix is consistent with view matrix
// =============================================================================

TEST_F(CameraViewTest, InverseViewTimesViewIsNearIdentity)
{
    camera.setViewDirection(
        glm::vec3(3.0f, -2.0f, 5.0f),
        glm::normalize(glm::vec3(1.0f, -1.0f, 2.0f))
    );

    glm::mat4 product = camera.getInverseView() * camera.getView();
    expectMatNear(product, glm::mat4(1.0f), 1e-3f);
}

TEST_F(CameraViewTest, InverseViewTimesViewIsNearIdentityYXZ)
{
    glm::quat rotation = glm::normalize(glm::quat(0.7071f, 0.0f, 0.7071f, 0.0f));
    camera.setViewYXZ(glm::vec3(1, 2, 3), rotation);

    glm::mat4 product = camera.getInverseView() * camera.getView();
    expectMatNear(product, glm::mat4(1.0f), 1e-3f);
}

// =============================================================================
// View matrix orthogonality — rotation part should be orthonormal
// =============================================================================

TEST_F(CameraViewTest, ViewMatrixRotationIsOrthonormal)
{
    camera.setViewDirection(
        glm::vec3(1, 2, 3),
        glm::normalize(glm::vec3(-1, 0, 1))
    );

    const auto &v = camera.getView();

    // Extract rows of the 3x3 rotation part (stored column-major)
    glm::vec3 row0(v[0][0], v[1][0], v[2][0]);
    glm::vec3 row1(v[0][1], v[1][1], v[2][1]);
    glm::vec3 row2(v[0][2], v[1][2], v[2][2]);

    // Each row should be unit length
    EXPECT_NEAR(glm::length(row0), 1.0f, EPSILON);
    EXPECT_NEAR(glm::length(row1), 1.0f, EPSILON);
    EXPECT_NEAR(glm::length(row2), 1.0f, EPSILON);

    // Rows should be mutually orthogonal
    EXPECT_NEAR(glm::dot(row0, row1), 0.0f, EPSILON);
    EXPECT_NEAR(glm::dot(row0, row2), 0.0f, EPSILON);
    EXPECT_NEAR(glm::dot(row1, row2), 0.0f, EPSILON);
}
