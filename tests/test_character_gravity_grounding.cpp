// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Engine/Body.h"
#include "Engine/Character.h"
#include "Engine/TerrainCollider.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace Sandbox3D;

// Helper to simulate scene-level collision resolution matching Sandbox::ResolveCollisions
static void SimulatePhysicsStep(Engine::Character& character, const Engine::TerrainCollider& terrainCollider, float dt)
{
    character.Update(dt);

    Maths::Vec3D pos = character.GetPosition();
    Maths::Vec3D vel = character.GetVelocity();
    const auto collider = character.GetCollider();
    if (collider && collider->IsCapsule())
    {
        const auto* capsule = static_cast<const Engine::CapsuleCollider*>(collider.get());
        const Maths::BoundingCapsuleD worldCapsule = capsule->GetWorldBoundingCapsule(character.GetWorldMatrix());
        const Engine::TerrainContact contact = terrainCollider.TestCapsule(worldCapsule);

        if (contact.hasContact)
        {
            pos.y += contact.penetrationDepth;
            character.SetPosition(pos);
            if (vel.y < 0.0)
            {
                vel.y = 0.0;
            }
            character.SetVelocity(vel);
            character.SetGrounded(true);
        }
        else
        {
            const double lowestY = std::min(worldCapsule.point0.y, worldCapsule.point1.y) - worldCapsule.radius;
            constexpr double maxStepDown = 0.15; // 15 cm step-down allowance for walking smoothly downhill
            if (character.IsGrounded() && (lowestY - contact.groundHeight) <= maxStepDown && vel.y <= 0.0)
            {
                pos.y -= (lowestY - contact.groundHeight);
                character.SetPosition(pos);
                vel.y = 0.0;
                character.SetVelocity(vel);
                character.SetGrounded(true);
            }
            else
            {
                character.SetGrounded(lowestY - contact.groundHeight <= 0.02);
            }
        }
    }
}

int main()
{
    std::cout << "=== Running Character Gravity & Grounding Verification ===\n\n";

    // Create a TerrainCollider with a flat height of 25.0m
    constexpr double testGroundHeight = 25.0;
    auto terrainCollider = std::make_shared<Engine::TerrainCollider>(
        1000.0,
        1000.0,
        [](double /*x*/, double /*z*/) { return testGroundHeight; },
        -100.0,
        500.0
    );

    assert(std::abs(terrainCollider->GetHeightAt(0.0, 0.0) - testGroundHeight) < 1e-5);
    std::cout << "[Test 1] TerrainCollider verified at elevation: " << testGroundHeight << "m\n";

    // Instantiate Character and verify capsule collider configuration
    auto character = std::make_shared<Engine::Character>("TestCharacter");
    assert(character->GetCollider() != nullptr && "Character should have a spatial collider!");
    assert(character->GetCollider()->IsCapsule() && "Character collider must be a capsule!");
    std::cout << "[Test 2] Character instantiated with capsule collider.\n";

    // Spawn character at ground height + 0.02m clearance matching Sandbox initialisation
    constexpr double spawnClearance = 0.02;
    character->SetPosition(Maths::Vec3D(0.0, testGroundHeight + spawnClearance, 0.0));
    assert(std::abs(character->GetPosition().y - (testGroundHeight + spawnClearance)) < 1e-6);

    // Simulate physics step-by-step
    constexpr float dt = 1.0f / 60.0f; // 60 UPS
    std::cout << "[Test 3] Simulating fall from clearance (+2cm) over 10 ticks...\n";
    for (int tick = 0; tick < 10; ++tick)
    {
        SimulatePhysicsStep(*character, *terrainCollider, dt);
    }

    std::cout << "  Position Y after 10 ticks: " << character->GetPosition().y << "m\n";
    std::cout << "  Velocity Y after 10 ticks: " << character->GetVelocity().y << " m/s\n";
    std::cout << "  IsGrounded: " << (character->IsGrounded() ? "true" : "false") << "\n";

    assert(std::abs(character->GetPosition().y - testGroundHeight) < 1e-5 && "Character position Y should equal ground height!");
    assert(std::abs(character->GetVelocity().y) < 1e-5 && "Vertical velocity should be 0 on ground!");
    assert(character->IsGrounded() && "Character must be grounded!");

    // Simulate additional ticks on ground to ensure zero jitter and no sinking
    std::cout << "[Test 4] Simulating 100 ticks standing on ground...\n";
    for (int tick = 0; tick < 100; ++tick)
    {
        SimulatePhysicsStep(*character, *terrainCollider, dt);
        assert(std::abs(character->GetPosition().y - testGroundHeight) < 1e-5 && "Character must not sink or drift!");
        assert(std::abs(character->GetVelocity().y) < 1e-5 && "Vertical velocity must remain 0!");
        assert(character->IsGrounded() && "Character must remain grounded!");
    }
    std::cout << "  Passed: Character remained rock-solid at ground elevation for 100 consecutive frames.\n";

    // Test dropping character from 10 metres high
    std::cout << "[Test 5] Dropping character from 10m above ground (elevation 35m)...\n";
    character->SetPosition(Maths::Vec3D(0.0, testGroundHeight + 10.0, 0.0));
    character->SetVelocity(Maths::Vec3D(0.0, 0.0, 0.0));

    // Fall time for 10m under g = 9.80655 m/s^2: t = sqrt(2*h/g) = sqrt(20 / 9.80655) ~ 1.428s ~ 86 frames
    bool hasLanded = false;
    for (int tick = 0; tick < 120; ++tick)
    {
        SimulatePhysicsStep(*character, *terrainCollider, dt);
        if (character->IsGrounded() && std::abs(character->GetPosition().y - testGroundHeight) < 1e-4)
        {
            hasLanded = true;
            std::cout << "  Landed at tick " << tick << " (simulated time: " << (tick * dt) << "s)\n";
            break;
        }
    }
    assert(hasLanded && "Character must land on terrain!");
    assert(std::abs(character->GetPosition().y - testGroundHeight) < 1e-5 && "Character must be at ground height after landing!");
    assert(character->GetVelocity().y == 0.0 && "Vertical velocity must be zeroed upon landing!");
    std::cout << "  Passed: High fall correctly resolved without falling through terrain.\n";

    // Test sloped terrain
    std::cout << "[Test 6] Testing contact on variable terrain elevations...\n";
    auto variableTerrain = std::make_shared<Engine::TerrainCollider>(
        1000.0,
        1000.0,
        [](double x, double /*z*/) { return 10.0 + x * 0.1; }, // slope: 1m rise per 10m
        -100.0,
        500.0
    );
    character->SetPosition(Maths::Vec3D(50.0, 20.0, 0.0)); // ground height at x=50 is 15.0m
    character->SetVelocity(Maths::Vec3D(0.0, 0.0, 0.0));

    for (int tick = 0; tick < 100; ++tick)
    {
        SimulatePhysicsStep(*character, *variableTerrain, dt);
    }
    const double expectedSlopeHeight = 15.0;
    assert(std::abs(character->GetPosition().y - expectedSlopeHeight) < 1e-4 && "Character must ground correctly on slope!");
    std::cout << "  Passed: Character grounded correctly on sloped terrain at Y = " << character->GetPosition().y << "m\n";

    // Verify head and camera synchronisation
    std::cout << "[Test 7] Verifying head and eye camera synchronisation...\n";
    const Maths::Vec3D headPos = character->GetHeadPosition();
    const Maths::Vec3D eyePos = character->GetEyePosition();
    assert(std::abs(headPos.y - (expectedSlopeHeight + character->GetHeadPivotHeight())) < 1e-4);
    assert(std::abs(eyePos.y - headPos.y) < 1e-4);
    std::cout << "  Passed: Head at Y=" << headPos.y << "m, Eye camera at Y=" << eyePos.y << "m.\n";

    // Verify player walking with WASD and body pointing directly upwards towards Y axis
    std::cout << "[Test 8] Verifying player walking locomotion & strictly vertical body orientation...\n";
    character->SetPosition(Maths::Vec3D(0.0, 10.0, 0.0)); // On variableTerrain at x=0, ground height = 10.0
    character->SetVelocity(Maths::Vec3D(0.0, 0.0, 0.0));
    character->SetOrientation(0.0, 0.5); // Looking up with pitch = 0.5 rad (~28 deg)

    // Verify body up vector in world matrix is strictly (0, 1, 0)
    const Maths::Mat4x4D& initialBodyTransform = character->GetBodyTransform();
    assert(std::abs(initialBodyTransform.m[1][0]) < 1e-6);
    assert(std::abs(initialBodyTransform.m[1][1] - 1.0) < 1e-6);
    assert(std::abs(initialBodyTransform.m[1][2]) < 1e-6);
    assert(std::abs(initialBodyTransform.m[1][3]) < 1e-6);

    // Walk forward along +Z at 5 m/s for 60 ticks (1 second)
    const Maths::Vec3D walkDir = character->GetWalkForward();
    assert(std::abs(walkDir.y) < 1e-6 && "Walk forward vector must have no vertical component!");
    character->SetHorizontalSpeed(walkDir * 5.0);

    for (int tick = 0; tick < 60; ++tick)
    {
        SimulatePhysicsStep(*character, *variableTerrain, dt);

        // Body must ALWAYS point directly upwards towards the Y axis
        const Maths::Mat4x4D& bodyTransform = character->GetBodyTransform();
        assert(std::abs(bodyTransform.m[1][0]) < 1e-6 && "Body must not roll!");
        assert(std::abs(bodyTransform.m[1][1] - 1.0) < 1e-6 && "Body up vector must remain 1.0 along Y!");
        assert(std::abs(bodyTransform.m[1][2]) < 1e-6 && "Body must not pitch!");
    }
    assert(character->GetPosition().z > 4.5 && "Character must have walked forward ~5m along Z!");
    assert(character->IsGrounded() && "Character must remain grounded while walking!");
    std::cout << "  Passed: Character walked 5m along Z while body remained strictly upright (Up = (0, 1, 0)).\n";

    // Walk up and down slope, verifying body stays pointing directly upwards towards Y
    std::cout << "[Test 9] Walking uphill along +X across slope (10% gradient)...\n";
    character->SetYaw(std::numbers::pi * 0.5); // Face +X
    const Maths::Vec3D uphillDir = character->GetWalkForward();
    assert(std::abs(uphillDir.x - 1.0) < 1e-5 && std::abs(uphillDir.z) < 1e-5);
    character->SetHorizontalSpeed(uphillDir * 5.0);

    for (int tick = 0; tick < 60; ++tick)
    {
        SimulatePhysicsStep(*character, *variableTerrain, dt);

        const Maths::Mat4x4D& bodyTransform = character->GetBodyTransform();
        assert(std::abs(bodyTransform.m[1][0]) < 1e-6 && "Body must not lean on slope!");
        assert(std::abs(bodyTransform.m[1][1] - 1.0) < 1e-6 && "Body up vector must remain exactly 1.0 along Y!");
        assert(std::abs(bodyTransform.m[1][2]) < 1e-6 && "Body must not pitch on slope!");
        assert(character->IsGrounded() && "Character must remain grounded while climbing slope!");
    }
    std::cout << "  Passed: Walked uphill to X=" << character->GetPosition().x << "m, Y=" << character->GetPosition().y
              << "m with body pointing strictly upwards towards Y axis.\n";

    std::cout << "\n=== ALL CHARACTER GRAVITY, GROUNDING & WALKING TESTS PASSED! ===\n";
    return 0;
}

