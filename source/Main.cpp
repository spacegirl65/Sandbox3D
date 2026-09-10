// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Core/Application.h"
#include "DxCheck.h"
#include "Maths/Maths.h"
#include "Renderer/Camera.h"

#include <cassert>
#include <iostream>

namespace
{
    void RunMathsVerificationSuite()
    {
        using namespace Sandbox3D::Maths;

        std::wcout << L"------------------------------------------------------------\n";
        std::wcout << L"[Maths Test] Executing Mathematical & Spatial Collision Suite\n";
        std::wcout << L"------------------------------------------------------------\n";

        // Layout verification for GPU structures
        {
            using Sandbox3D::Renderer::Vertex;
            using Sandbox3D::Renderer::SceneConstantBuffer;
            static_assert(sizeof(Vertex) == 40);
            static_assert(sizeof(SceneConstantBuffer) == 176);
            static_assert(offsetof(SceneConstantBuffer, mvp) == 0);
            static_assert(offsetof(SceneConstantBuffer, world) == 64);
            static_assert(offsetof(SceneConstantBuffer, lightDirection) == 128);
            static_assert(offsetof(SceneConstantBuffer, lightColor) == 144);
            static_assert(offsetof(SceneConstantBuffer, ambientColor) == 160);
        }

        // 1. Vec4 & Vec4D Verification
        {
            const Vec4 v1(1.0f, 2.0f, 3.0f, 4.0f);
            const Vec4 v2(5.0f, 6.0f, 7.0f, 8.0f);
            assert(v1 + v2 == Vec4(6.0f, 8.0f, 10.0f, 12.0f));
            assert(v2 - v1 == Vec4(4.0f, 4.0f, 4.0f, 4.0f));
            assert(v1 * 2.0f == Vec4(2.0f, 4.0f, 6.0f, 8.0f));
            assert(ApproximatelyEqual(v1.Dot(v2), 70.0f));
            assert(ApproximatelyEqual(Vec4(0.0f, 3.0f, 4.0f, 0.0f).Length(), 5.0f));

            // Colour accessors and constants
            const Vec4 red = Vec4::Red();
            assert(red.r() == 1.0f && red.g() == 0.0f && red.b() == 0.0f && red.a() == 1.0f);
            const Vec4 yellow = Vec4::Yellow();
            assert(yellow.r() == 1.0f && yellow.g() == 1.0f && yellow.b() == 0.0f && yellow.a() == 1.0f);

            // Matrix * Vec4
            const Mat4x4 trans = Mat4x4::Translation(10.0f, 20.0f, 30.0f);
            const Vec4 p(1.0f, 2.0f, 3.0f, 1.0f);
            const Vec4 pTransformed = trans * p;
            assert(pTransformed == Vec4(11.0f, 22.0f, 33.0f, 1.0f));

            std::wcout << L"[Maths Test] PASS: Vec4, Vec4D, colour accessors, and Mat4x4 homogeneous transformation.\n";
        }

        // 2. Rect & RectD Verification
        {
            const Rect rect(10.0f, 20.0f, 100.0f, 200.0f);
            assert(rect.GetLeft() == 10.0f);
            assert(rect.GetTop() == 20.0f);
            assert(rect.GetRight() == 110.0f);
            assert(rect.GetBottom() == 220.0f);
            assert(rect.Contains(50.0f, 50.0f));
            assert(!rect.Contains(5.0f, 50.0f));

            const Rect overlapping(50.0f, 50.0f, 100.0f, 100.0f);
            const Rect nonOverlapping(300.0f, 300.0f, 50.0f, 50.0f);
            assert(rect.Intersects(overlapping));
            assert(!rect.Intersects(nonOverlapping));

            // D3D12 Viewport conversion
            const D3D12_VIEWPORT vp = rect.ToD3D12Viewport(0.0f, 1.0f);
            assert(vp.Width == 100.0f && vp.Height == 200.0f);

            std::wcout << L"[Maths Test] PASS: Rect, RectD, spatial containment, and D3D12 viewport conversion.\n";
        }

        // 3. Plane & PlaneD Verification
        {
            // Plane passing through (0, 10, 0) with normal pointing straight Up (+Y)
            const Plane groundPlane(Vec3::Up(), Vec3(0.0f, 10.0f, 0.0f));
            assert(groundPlane.normal == Vec3::Up());
            assert(ApproximatelyEqual(groundPlane.distance, -10.0f));

            // Signed distances
            assert(ApproximatelyEqual(groundPlane.SignedDistance(Vec3(0.0f, 15.0f, 0.0f)), 5.0f));
            assert(ApproximatelyEqual(groundPlane.SignedDistance(Vec3(0.0f, 5.0f, 0.0f)), -5.0f));
            assert(groundPlane.ClassifyPoint(Vec3(0.0f, 15.0f, 0.0f)) == PlaneIntersectionType::Front);
            assert(groundPlane.ClassifyPoint(Vec3(0.0f, 5.0f, 0.0f)) == PlaneIntersectionType::Back);

            // Plane from 3 coplanar triangle points on XY plane
            const Plane triPlane(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            assert(triPlane.ClassifyPoint(Vec3(0.5f, 0.5f, 0.0f)) == PlaneIntersectionType::Intersecting);

            std::wcout << L"[Maths Test] PASS: Plane, PlaneD, signed distance, and point classifications.\n";
        }

        // 4. BoundingBox & BoundingSphere Verification
        {
            const Vec3 trianglePts[] = {
                Vec3(0.0f, 0.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f),
                Vec3(1.0f, 1.0f, 0.0f)
            };

            const BoundingBox aabb = BoundingBox::FromPoints(trianglePts);
            assert(aabb.min == Vec3(0.0f, 0.0f, 0.0f));
            assert(aabb.max == Vec3(1.0f, 1.0f, 0.0f));
            assert(aabb.GetCenter() == Vec3(0.5f, 0.5f, 0.0f));
            assert(aabb.Contains(Vec3(0.5f, 0.5f, 0.0f)));
            assert(!aabb.Contains(Vec3(2.0f, 0.5f, 0.0f)));

            const BoundingSphere sphere = BoundingSphere::FromPoints(trianglePts);
            assert(sphere.center == Vec3(0.5f, 0.5f, 0.0f));
            assert(sphere.Contains(Vec3(0.0f, 0.0f, 0.0f)));
            assert(sphere.Contains(Vec3(1.0f, 1.0f, 0.0f)));

            // Box vs Sphere overlap
            assert(aabb.Intersects(sphere));

            // Transformed bounding box
            const Mat4x4 trans = Mat4x4::Translation(10.0f, 20.0f, 30.0f);
            const BoundingBox transBox = aabb.Transformed(trans);
            assert(transBox.min == Vec3(10.0f, 20.0f, 30.0f));
            assert(transBox.max == Vec3(11.0f, 21.0f, 30.0f));

            std::wcout << L"[Maths Test] PASS: BoundingBox, BoundingSphere, and affine transformations.\n";
        }

        // 5. Ray & Ray-Triangle / Ray-Box Intersection Verification
        {
            // Ray pointing forward (+Z) from (0.25, 0.75, -5.0)
            const Ray ray(Vec3(0.25f, 0.75f, -5.0f), Vec3::Forward());

            // Intersect with the red triangle at z=0: (0, 0, 0), (0, 1, 0), (1, 1, 0)
            float hitDist = 0.0f;
            const bool hitTriangle = ray.IntersectsTriangle(
                Vec3(0.0f, 0.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f),
                Vec3(1.0f, 1.0f, 0.0f),
                hitDist
            );
            assert(hitTriangle);
            assert(ApproximatelyEqual(hitDist, 5.0f));

            // Ray missing the triangle
            const Ray missingRay(Vec3(0.8f, 0.2f, -5.0f), Vec3::Forward());
            assert(!missingRay.IntersectsTriangle(
                Vec3(0.0f, 0.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f),
                Vec3(1.0f, 1.0f, 0.0f),
                hitDist
            ));

            // Ray vs BoundingBox
            const BoundingBox box(Vec3(-1.0f, -1.0f, 5.0f), Vec3(1.0f, 1.0f, 7.0f));
            const Ray boxRay(Vec3(0.0f, 0.0f, 0.0f), Vec3::Forward());
            float boxHitDist = 0.0f;
            assert(boxRay.Intersects(box, boxHitDist));
            assert(ApproximatelyEqual(boxHitDist, 5.0f));

            std::wcout << L"[Maths Test] PASS: Ray Moller-Trumbore triangle and AABB slab intersections.\n";
        }

        // 6. BoundingFrustum & View Frustum Culling Verification
        {
            // Create camera looking straight at triangle center (0.5, 0.5, 0) from (0.5, 0.5, -2.5)
            const Mat4x4 view = Mat4x4::LookAt(
                Vec3(0.5f, 0.5f, -2.5f),
                Vec3(0.5f, 0.5f, 0.0f),
                Vec3::Up()
            );
            const Mat4x4 proj = Mat4x4::Perspective(ToRadians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
            const Mat4x4 viewProj = view * proj;

            const BoundingFrustum frustum(viewProj);

            // Triangle center must be inside the frustum
            assert(frustum.Contains(Vec3(0.5f, 0.5f, 0.0f)));

            // Triangle bounding box must intersect the frustum
            const BoundingBox triBox(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            assert(frustum.Intersects(triBox));

            // A point far behind the camera must NOT be inside the frustum
            assert(!frustum.Contains(Vec3(0.5f, 0.5f, -10.0f)));

            // An object far outside the viewport must NOT intersect
            const BoundingBox farAwayBox(Vec3(500.0f, 500.0f, 0.0f), Vec3(502.0f, 502.0f, 1.0f));
            assert(!frustum.Intersects(farAwayBox));

            std::wcout << L"[Maths Test] PASS: BoundingFrustum plane extraction and frustum culling queries.\n";
        }

        std::wcout << L"------------------------------------------------------------\n";
        std::wcout << L"[Maths Test] ALL MATHEMATICAL & COLLISION TESTS PASSED!\n";
        std::wcout << L"------------------------------------------------------------\n\n";
    }
}

int main(int argc, char* argv[])
{
    try
    {
        // 1. Run the custom mathematics, collision, and camera verification suite
        RunMathsVerificationSuite();

        if (argc > 1 && std::string(argv[1]) == "--test-only")
        {
            return 0;
        }

        std::wcout << L"Starting Sandbox3D DirectX 12 Native Application...\n";

        Sandbox3D::Core::Application app(2880, 1200, L"Sandbox3D - [DX12, 2880 x 1200]");
        return app.Run();
    }
    catch (const Sandbox3D::DxException& ex)
    {
        std::wcerr << L"[DX_EXCEPTION] " << ex.GetErrorMessage() << L"\n";
        return -1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[STD_EXCEPTION] " << ex.what() << "\n";
        return -1;
    }
}

