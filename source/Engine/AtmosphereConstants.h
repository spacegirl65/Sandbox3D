// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

#include <cmath>

namespace Sandbox3D::Engine
{
    // Physical atmospheric parameters for Rayleigh, Mie, and Ozone scattering
    struct AtmosphereParameters
    {
        // Rayleigh scattering coefficients (m^-1) in rgb, scale height in w (m)
        Maths::Vec4 rayleighParams{ 5.802e-6f, 13.558e-6f, 33.100e-6f, 8000.0f };

        // Mie scattering coefficients (m^-1) in rgb, scale height in w (m)
        Maths::Vec4 mieParams{ 3.996e-6f, 3.996e-6f, 3.996e-6f, 1200.0f };

        // Ozone absorption coefficients (m^-1) in rgb, Mie asymmetry parameter g in w
        Maths::Vec4 ozoneParams{ 0.650e-6f, 1.881e-6f, 0.085e-6f, 0.76f };

        // Planetary geometry and solar constants: x = planet radius (m), y = atmosphere radius (m), z = sun illuminance intensity, w = ozone center altitude (m)
        Maths::Vec4 planetParams{ 6360000.0f, 6420000.0f, 22.0f, 25000.0f };

        // Calculates unit vector towards celestial solar disc from azimuth and elevation angles (radians)
        [[nodiscard]] static Maths::Vec3 DirectionFromAzimuthElevation(float azimuthRadians, float elevationRadians) noexcept
        {
            const float cosElev = std::cos(elevationRadians);
            return Maths::Vec3(
                std::sin(azimuthRadians) * cosElev,
                std::sin(elevationRadians),
                std::cos(azimuthRadians) * cosElev
            ).Normalised();
        }
    };
}

