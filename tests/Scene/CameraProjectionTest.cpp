#include <gtest/gtest.h>
#include <cmath>
#include <glm/glm.hpp>
#include "Scene/Camera.h"

class CameraProjectionTest : public ::testing::Test {
protected:
    Engine::Camera camera;

    static constexpr float EPSILON = 1e-5f;
};

// =============================================================================
// Perspective projection
// =============================================================================

TEST_F(CameraProjectionTest, PerspectiveMatrixDiagonalElements)
{
    float fovy = glm::radians(45.0f);
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    camera.setPerspectiveProjection(fovy, aspect, nearPlane, farPlane);

    const auto &m = camera.getProjection();
    float tanHalfFov = tan(fovy / 2.0f);

    // M[0][0] = 1 / (aspect * tan(fov/2))
    EXPECT_NEAR(m[0][0], 1.0f / (aspect * tanHalfFov), EPSILON);

    // M[1][1] = 1 / tan(fov/2)
    EXPECT_NEAR(m[1][1], 1.0f / tanHalfFov, EPSILON);

    // M[2][2] = far / (far - near)
    EXPECT_NEAR(m[2][2], farPlane / (farPlane - nearPlane), EPSILON);

    // M[2][3] = 1 (for perspective divide)
    EXPECT_NEAR(m[2][3], 1.0f, EPSILON);

    // M[3][2] = -(far * near) / (far - near)
    EXPECT_NEAR(m[3][2], -(farPlane * nearPlane) / (farPlane - nearPlane), EPSILON);
}

TEST_F(CameraProjectionTest, PerspectiveStoresNearAndFar)
{
    camera.setPerspectiveProjection(glm::radians(60.0f), 1.0f, 0.5f, 500.0f);

    EXPECT_FLOAT_EQ(camera.getNearClip(), 0.5f);
    EXPECT_FLOAT_EQ(camera.getFarClip(), 500.0f);
}

TEST_F(CameraProjectionTest, PerspectiveIsNotIdentity)
{
    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);

    const auto &m = camera.getProjection();
    EXPECT_NE(m, glm::mat4(1.0f));
}

TEST_F(CameraProjectionTest, PerspectiveOffDiagonalAreZero)
{
    camera.setPerspectiveProjection(glm::radians(90.0f), 2.0f, 1.0f, 1000.0f);

    const auto &m = camera.getProjection();

    // Most off-diagonal should be zero in a standard perspective matrix
    EXPECT_NEAR(m[0][1], 0.0f, EPSILON);
    EXPECT_NEAR(m[0][2], 0.0f, EPSILON);
    EXPECT_NEAR(m[0][3], 0.0f, EPSILON);
    EXPECT_NEAR(m[1][0], 0.0f, EPSILON);
    EXPECT_NEAR(m[1][2], 0.0f, EPSILON);
    EXPECT_NEAR(m[1][3], 0.0f, EPSILON);
    EXPECT_NEAR(m[2][0], 0.0f, EPSILON);
    EXPECT_NEAR(m[2][1], 0.0f, EPSILON);
    EXPECT_NEAR(m[3][0], 0.0f, EPSILON);
    EXPECT_NEAR(m[3][1], 0.0f, EPSILON);
    EXPECT_NEAR(m[3][3], 0.0f, EPSILON);
}

TEST_F(CameraProjectionTest, PerspectiveAspectRatioAffectsHorizontalScale)
{
    float fovy = glm::radians(45.0f);
    float near = 0.1f, far = 100.0f;

    Engine::Camera cam1, cam2;
    cam1.setPerspectiveProjection(fovy, 1.0f, near, far);
    cam2.setPerspectiveProjection(fovy, 2.0f, near, far);

    // Wider aspect → smaller M[0][0]
    EXPECT_GT(cam1.getProjection()[0][0], cam2.getProjection()[0][0]);

    // M[1][1] should be the same (vertical FOV unchanged)
    EXPECT_NEAR(cam1.getProjection()[1][1], cam2.getProjection()[1][1], EPSILON);
}

TEST_F(CameraProjectionTest, PerspectiveWiderFOVReducesScale)
{
    float aspect = 16.0f / 9.0f;
    float near = 0.1f, far = 100.0f;

    Engine::Camera cam1, cam2;
    cam1.setPerspectiveProjection(glm::radians(45.0f), aspect, near, far);
    cam2.setPerspectiveProjection(glm::radians(90.0f), aspect, near, far);

    // Wider FOV → smaller diagonal elements
    EXPECT_GT(cam1.getProjection()[0][0], cam2.getProjection()[0][0]);
    EXPECT_GT(cam1.getProjection()[1][1], cam2.getProjection()[1][1]);
}

// =============================================================================
// Orthographic projection
// =============================================================================

TEST_F(CameraProjectionTest, OrthographicMatrixElements)
{
    float left = -10.0f, right = 10.0f;
    float top = -10.0f, bottom = 10.0f;
    float near = 0.1f, far = 100.0f;

    camera.setOrthographicProjection(left, right, top, bottom, near, far);

    const auto &m = camera.getProjection();

    // M[0][0] = 2 / (right - left)
    EXPECT_NEAR(m[0][0], 2.0f / (right - left), EPSILON);

    // M[1][1] = 2 / (bottom - top)
    EXPECT_NEAR(m[1][1], 2.0f / (bottom - top), EPSILON);

    // M[2][2] = 1 / (far - near)
    EXPECT_NEAR(m[2][2], 1.0f / (far - near), EPSILON);

    // M[3][0] = -(right + left) / (right - left)
    EXPECT_NEAR(m[3][0], -(right + left) / (right - left), EPSILON);

    // M[3][1] = -(bottom + top) / (bottom - top)
    EXPECT_NEAR(m[3][1], -(bottom + top) / (bottom - top), EPSILON);

    // M[3][2] = -near / (far - near)
    EXPECT_NEAR(m[3][2], -near / (far - near), EPSILON);
}

TEST_F(CameraProjectionTest, OrthographicStoresNearAndFar)
{
    camera.setOrthographicProjection(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 50.0f);

    EXPECT_FLOAT_EQ(camera.getNearClip(), 1.0f);
    EXPECT_FLOAT_EQ(camera.getFarClip(), 50.0f);
}

TEST_F(CameraProjectionTest, OrthographicSymmetricCentersTranslationAtZero)
{
    // Symmetric: left = -right, top = -bottom
    camera.setOrthographicProjection(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f);

    const auto &m = camera.getProjection();

    // Translation components should be zero for symmetric volumes
    EXPECT_NEAR(m[3][0], 0.0f, EPSILON);
    EXPECT_NEAR(m[3][1], 0.0f, EPSILON);
}

TEST_F(CameraProjectionTest, OrthographicNoPerspectiveDivide)
{
    camera.setOrthographicProjection(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);

    const auto &m = camera.getProjection();

    // For orthographic, M[2][3] should be 0 (no perspective divide)
    EXPECT_NEAR(m[2][3], 0.0f, EPSILON);
    // M[3][3] should be 1
    EXPECT_NEAR(m[3][3], 1.0f, EPSILON);
}

// =============================================================================
// Overwriting projection
// =============================================================================

TEST_F(CameraProjectionTest, SettingPerspectiveOverwritesPrevious)
{
    camera.setOrthographicProjection(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 100.0f);
    const auto orthoMat = camera.getProjection();

    camera.setPerspectiveProjection(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    const auto perspMat = camera.getProjection();

    EXPECT_NE(orthoMat, perspMat);
}
