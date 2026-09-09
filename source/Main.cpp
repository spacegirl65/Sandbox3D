// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "GraphicsEngine.h"
#include "Maths/Maths.h"

#include <cassert>
#include <iostream>

namespace
{
    void RunMathsVerificationSuite()
    {
        using namespace Sandbox3D::Maths;

        std::wcout << L"------------------------------------------------------------\n";
        std::wcout << L"[Maths Test] Executing Custom Mathematics Verification Suite\n";
        std::wcout << L"------------------------------------------------------------\n";

        // 1. Vec2, Vec2D, and Vec2I Validation
        {
            const Vec2 a(3.0f, 4.0f);
            assert(ApproximatelyEqual(a.Length(), 5.0f));
            assert(ApproximatelyEqual(a.LengthSquared(), 25.0f));

            const Vec2 norm = a.Normalised();
            assert(ApproximatelyEqual(norm.Length(), 1.0f));

            const Vec2 b(1.0f, 0.0f);
            assert(ApproximatelyEqual(a.Dot(b), 3.0f));
            assert(ApproximatelyEqual(a.Cross(b), -4.0f)); // 2D perp-dot

            // Double precision Vec2D
            const Vec2D d1(1e12, 2e12);
            const Vec2D d2(1e12, 2e12 + 3.0);
            assert(ApproximatelyEqual(d1.Distance(d2), 3.0));

            // Integer Vec2I for screen / viewport coordinates
            const Vec2I pixelCoord(1920, 1080);
            const Vec2I offset(10, -20);
            const Vec2I resultPixel = pixelCoord + offset;
            assert(resultPixel.x == 1930 && resultPixel.y == 1060);

            // Multi-precision conversion
            const Vec2 floatFromInt(pixelCoord);
            assert(ApproximatelyEqual(floatFromInt.x, 1920.0f) && ApproximatelyEqual(floatFromInt.y, 1080.0f));

            std::wcout << L"[Maths Test] PASS: Vec2, Vec2D, and Vec2I operations.\n";
        }

        // 2. Vec3 and Vec3D Left-Handed (LH) Validation
        {
            const Vec3 forward  = Vec3::Forward();  // (0, 0, 1)
            const Vec3 backward = Vec3::Backward(); // (0, 0, -1)
            const Vec3 up       = Vec3::Up();        // (0, 1, 0)
            const Vec3 down     = Vec3::Down();      // (0, -1, 0)
            const Vec3 right    = Vec3::Right();     // (1, 0, 0)
            const Vec3 left     = Vec3::Left();      // (-1, 0, 0)

            assert(forward.z == 1.0f && forward.x == 0.0f && forward.y == 0.0f);
            assert(backward.z == -1.0f);
            assert(up.y == 1.0f && down.y == -1.0f);
            assert(right.x == 1.0f && left.x == -1.0f);

            // Left-Handed cross product orientation: Right x Up = Forward
            const Vec3 crossForward = right.Cross(up);
            assert(crossForward == forward);

            // Geometric operations
            const Vec3 v(1.0f, 2.0f, 2.0f);
            assert(ApproximatelyEqual(v.Length(), 3.0f));

            const Vec3 reflected = Vec3(1.0f, -1.0f, 0.0f).Reflect(Vec3::Up());
            assert(ApproximatelyEqual(reflected.y, 1.0f));

            std::wcout << L"[Maths Test] PASS: Vec3 and Vec3D Left-Handed directions and cross products.\n";
        }

        // 3. Mat3x3 and Mat3x3D Validation
        {
            const Mat3x3 identity = Mat3x3::Identity();
            assert(ApproximatelyEqual(identity.Determinant(), 1.0f));

            // Rotation around X by 90 degrees
            const Mat3x3 rotX = Mat3x3::RotationX(HalfPi<float>);
            const Vec3 rotatedUp = rotX.TransformVector(Vec3::Up());
            // In LH, rotating Up (+Y) around +X by +90 deg curls to Forward (+Z)
            assert(ApproximatelyEqual(rotatedUp.x, 0.0f) &&
                   ApproximatelyEqual(rotatedUp.y, 0.0f) &&
                   ApproximatelyEqual(rotatedUp.z, 1.0f));

            // Invertibility check
            const Mat3x3 rotY = Mat3x3::RotationY(ToRadians(45.0f));
            const Mat3x3 invRotY = rotY.Inverted();
            const Mat3x3 shouldBeIdentity = rotY * invRotY;
            assert(shouldBeIdentity == Mat3x3::Identity());

            std::wcout << L"[Maths Test] PASS: Mat3x3 rotations, determinants, and inversion.\n";
        }

        // 4. Mat4x4 and Mat4x4D Validation (DirectX Left-Handed Systems)
        {
            // Translation
            const Mat4x4 trans = Mat4x4::Translation(10.0f, 20.0f, 30.0f);
            const Vec3 origin(0.0f, 0.0f, 0.0f);
            const Vec3 translatedPoint = trans.TransformPoint(origin);
            assert(ApproximatelyEqual(translatedPoint.x, 10.0f) &&
                   ApproximatelyEqual(translatedPoint.y, 20.0f) &&
                   ApproximatelyEqual(translatedPoint.z, 30.0f));

            // Direction vectors must ignore translation (w=0)
            const Vec3 dir(1.0f, 0.0f, 0.0f);
            const Vec3 translatedDir = trans.TransformDirection(dir);
            assert(translatedDir == dir);

            // Inversion test (Affine M * M^-1 = I)
            const Mat4x4 compound = Mat4x4::Translation(5.0f, -3.0f, 12.0f) *
                                   Mat4x4::RotationYawPitchRoll(0.3f, -0.2f, 0.5f) *
                                   Mat4x4::Scale(2.0f, 3.0f, 4.0f);
            const Mat4x4 invCompound = compound.Inverted();
            const Mat4x4 product = compound * invCompound;
            assert(product == Mat4x4::Identity());

            // LookAtLH Left-Handed Camera
            const Vec3 eye(0.0f, 0.0f, -10.0f);
            const Vec3 target(0.0f, 0.0f, 0.0f);
            const Vec3 up(0.0f, 1.0f, 0.0f);
            const Mat4x4 view = Mat4x4::LookAtLH(eye, target, up);
            const Vec3 targetInView = view.TransformPoint(target);
            assert(ApproximatelyEqual(targetInView.x, 0.0f) &&
                   ApproximatelyEqual(targetInView.y, 0.0f) &&
                   ApproximatelyEqual(targetInView.z, 10.0f)); // Target is 10 units forward (+Z)

            // PerspectiveFovLH with DirectX [0, 1] Clip Depth
            const float nearZ = 0.1f;
            const float farZ  = 1000.0f;
            const Mat4x4 proj = Mat4x4::PerspectiveFovLH(ToRadians(60.0f), 16.0f / 9.0f, nearZ, farZ);

            // Near plane maps to Z = 0 in clip space
            const Vec3 pointNear(0.0f, 0.0f, nearZ);
            const Vec3 clipNear = proj.TransformPoint(pointNear);
            assert(ApproximatelyEqual(clipNear.z, 0.0f));

            // Far plane maps to Z = 1 in clip space
            const Vec3 pointFar(0.0f, 0.0f, farZ);
            const Vec3 clipFar = proj.TransformPoint(pointFar);
            assert(ApproximatelyEqual(clipFar.z, 1.0f));

            std::wcout << L"[Maths Test] PASS: Mat4x4 Affine, LookAtLH, and PerspectiveFovLH ([0, 1] depth).\n";
        }

        // 5. Dual-Tier Coordinate Infrastructure & Camera-Relative Rendering (Rules 19 & 20)
        {
            // Entity positioned at astronomical distance (100,000,000,000 metres)
            constexpr double astronomicalDistance = 100'000'000'000.0;
            const Mat4x4D worldTransform = Mat4x4D::Translation(astronomicalDistance, 0.0, 0.0);

            // Camera placed 12.5 metres away from the entity
            const Vec3D cameraPosition(astronomicalDistance + 12.5, 0.0, 0.0);

            // Camera looking directly at origin along X
            const Mat4x4D viewMatrix = Mat4x4D::LookAtLH(
                cameraPosition,
                Vec3D(astronomicalDistance, 0.0, 0.0),
                Vec3D::Up()
            );

            // Generate camera-relative Model-View matrix
            const Mat4x4 cameraRelativeMV = Mat4x4::CreateCameraRelativeModelView(
                worldTransform,
                cameraPosition,
                viewMatrix
            );

            // Entity point (origin of entity) in camera-relative view space
            const Vec3 entityInView = cameraRelativeMV.TransformPoint(Vec3::Zero());

            // Because the camera is 12.5m away looking directly at the entity,
            // the entity should be exactly at Z = 12.5m in view space!
            assert(ApproximatelyEqual(entityInView.x, 0.0f) &&
                   ApproximatelyEqual(entityInView.y, 0.0f) &&
                   ApproximatelyEqual(entityInView.z, 12.5f));

            std::wcout << L"[Maths Test] PASS: Dual-tier camera-relative rendering (eliminated astronomical jitter).\n";
        }

        std::wcout << L"------------------------------------------------------------\n";
        std::wcout << L"[Maths Test] ALL MATHEMATICS VERIFICATION TESTS PASSED!\n";
        std::wcout << L"------------------------------------------------------------\n\n";
    }
}

int main(int argc, char* argv[])
{
    try
    {
        // 1. Run the custom mathematics verification suite
        RunMathsVerificationSuite();

        // 2. Initialise and test the DirectX 12 Graphics Engine
        std::wcout << L"Initialising Sandbox3D Graphics Subsystem...\n";

        Sandbox3D::GraphicsEngine engine;
        engine.Initialise(/* enableDebugLayer = */ true);

        std::wcout << L"Graphics Engine initialisation complete.\n";
        std::wcout << L"Active GPU: " << engine.GetGpuDescription() << L"\n";

        // Clean up graphics engine
        engine.Shutdown();
    }
    catch (const Sandbox3D::DxException& ex)
    {
        std::wcerr << L"[EXCEPTION] " << ex.GetErrorMessage() << L"\n";
        return -1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[EXCEPTION] " << ex.what() << "\n";
        return -1;
    }

    return 0;
}

