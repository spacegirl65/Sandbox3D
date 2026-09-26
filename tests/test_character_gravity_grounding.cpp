// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Engine/Body.h"
#include "Engine/Character.h"
#include "Engine/TerrainCollider.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace Sandbox3D;

int main()
{
    std::cout << "=== Running Character Gravity & Grounding Verification ===\n\n";

    // 1. Create a TerrainCollider with a flat height of 25.0m
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

    // 2. Instantiate Character and verify gravity and capsule collider configuration
    auto character = std::make_shared<Engine::Character>("TestCharacter");
    assert(character->IsUsingGravity() && "Character should have gravity enabled by default!");
    assert(character->GetCollider() != nullptr && "Character should have a spatial collider!");
    assert(character->GetCollider()->IsCapsule() && "Character collider must be a capsule!");

    character->SetTerrainCollider(terrainCollider);
    assert(character->GetTerrainCollider() == terrainCollider && "TerrainCollider must be attached to character!");
    std::cout << "[Test 2] Character instantiated with capsule collider & terrain collider attached.\n";

    // 3. Spawn character at ground height + 0.02m clearance (same as Sandbox::Initialise)
    constexpr double spawnClearance = 0.02;
    character->SetPosition(Maths::Vec3D(0.0, testGroundHeight + spawnClearance, 0.0));
    assert(std::abs(character->GetPosition().y - (testGroundHeight + spawnClearance)) < 1e-6);

    // 4. Simulate physics step-by-step
    constexpr float dt = 1.0f / 60.0f; // 60 UPS
    std::cout << "[Test 3] Simulating fall from clearance (+2cm) over 10 ticks...\n";
    for (int tick = 0; tick < 10; ++tick)
    {
        character->Update(dt);
    }

    std::cout << "  Position Y after 10 ticks: " << character->GetPosition().y << "m\n";
    std::cout << "  Speed Y after 10 ticks: " << character->GetSpeed().y << " m/s\n";
    std::cout << "  IsGrounded: " << (character->IsGrounded() ? "true" : "false") << "\n";

    assert(std::abs(character->GetPosition().y - testGroundHeight) < 1e-5 && "Character position Y should equal ground height!");
    assert(std::abs(character->GetSpeed().y) < 1e-5 && "Vertical speed should be 0 on ground!");
    assert(character->IsGrounded() && "Character must be grounded!");

    // 5. Simulate 100 more ticks on ground to ensure zero jitter and no sinking
    std::cout << "[Test 4] Simulating 100 ticks standing on ground...\n";
    for (int tick = 0; tick < 100; ++tick)
    {
        character->Update(dt);
        assert(std::abs(character->GetPosition().y - testGroundHeight) < 1e-5 && "Character must not sink or drift!");
        assert(std::abs(character->GetSpeed().y) < 1e-5 && "Vertical speed must remain 0!");
        assert(character->IsGrounded() && "Character must remain grounded!");
    }
    std::cout << "  Passed: Character remained rock-solid at ground elevation for 100 consecutive frames.\n";

    // 6. Test dropping character from 10 metres high
    std::cout << "[Test 5] Dropping character from 10m above ground (elevation 35m)...\n";
    character->SetPosition(Maths::Vec3D(0.0, testGroundHeight + 10.0, 0.0));
    character->SetSpeed(Maths::Vec3D(0.0, 0.0, 0.0));

    // Fall time for 10m under g = 9.80655 m/s^2: t = sqrt(2*h/g) = sqrt(20 / 9.80655) ~ 1.428s ~ 86 frames
    bool hasLanded = false;
    for (int tick = 0; tick < 120; ++tick)
    {
        character->Update(dt);
        if (character->IsGrounded() && std::abs(character->GetPosition().y - testGroundHeight) < 1e-4)
        {
            hasLanded = true;
            std::cout << "  Landed at tick " << tick << " (simulated time: " << (tick * dt) << "s)\n";
            break;
        }
    }
    assert(hasLanded && "Character must land on terrain!");
    assert(std::abs(character->GetPosition().y - testGroundHeight) < 1e-5 && "Character must be at ground height after landing!");
    assert(character->GetSpeed().y == 0.0 && "Vertical velocity must be zeroed upon landing!");
    std::cout << "  Passed: High fall correctly resolved without falling through terrain.\n";

    // 7. Test sloped terrain
    std::cout << "[Test 6] Testing contact on variable terrain elevations...\n";
    auto variableTerrain = std::make_shared<Engine::TerrainCollider>(
        1000.0,
        1000.0,
        [](double x, double /*z*/) { return 10.0 + x * 0.1; }, // slope: 1m rise per 10m
        -100.0,
        500.0
    );
    character->SetTerrainCollider(variableTerrain);
    character->SetPosition(Maths::Vec3D(50.0, 20.0, 0.0)); // ground height at x=50 is 15.0m
    character->SetSpeed(Maths::Vec3D(0.0, 0.0, 0.0));

    for (int tick = 0; tick < 100; ++tick)
    {
        character->Update(dt);
    }
    const double expectedSlopeHeight = 15.0;
    assert(std::abs(character->GetPosition().y - expectedSlopeHeight) < 1e-4 && "Character must ground correctly on slope!");
    std::cout << "  Passed: Character grounded correctly on sloped terrain at Y = " << character->GetPosition().y << "m\n";

    // 8. Verify head and camera synchronisation
    std::cout << "[Test 7] Verifying head and eye camera synchronisation...\n";
    const Maths::Vec3D headPos = character->GetHeadPosition();
    const Maths::Vec3D eyePos = character->GetEyePosition();
    assert(std::abs(headPos.y - (expectedSlopeHeight + character->GetHeadPivotHeight())) < 1e-4);
    assert(std::abs(eyePos.y - headPos.y) < 1e-4);
    std::cout << "  Passed: Head at Y=" << headPos.y << "m, Eye camera at Y=" << eyePos.y << "m.\n";

    std::cout << "\n=== ALL CHARACTER GRAVITY & GROUNDING TESTS PASSED! ===\n";
    return 0;
}

