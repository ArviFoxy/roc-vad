/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "driver/volume_control.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace rocvad;

namespace {

// librespot's LogMapping, transcribed from playback/src/mixer/mappings.rs:
//
//     let (db_ratio, ideal_factor) = coefficients(db_range);   // 10^(r/20), ln(that)
//     exp(ideal_factor * normalized_volume) / db_ratio
//
// That mapping is what this taper exists to match, so the test compares against
// it directly rather than against a restatement of our own arithmetic.
double librespot_gain(double scalar, double db_range = VolumeRangeDb)
{
    const double db_ratio = std::pow(10.0, db_range / 20.0);
    const double ideal_factor = std::log(db_ratio);

    return std::exp(ideal_factor * scalar) / db_ratio;
}

} // namespace

// The mapping libASPL's stock control does not apply: scalar linear in decibels.
TEST(VolumeControlTest, scalar_to_decibels_is_linear)
{
    for (int i = 0; i <= 100; i++) {
        const float scalar = float(i) / 100.0f;

        EXPECT_NEAR(volume_scalar_to_decibels(scalar), -60.0f * (1.0f - scalar), 1e-3)
            << "at scalar " << scalar;
    }
}

TEST(VolumeControlTest, gain_matches_librespot)
{
    for (int i = 1; i < 100; i++) {
        const float scalar = float(i) / 100.0f;

        EXPECT_NEAR(volume_scalar_to_gain(scalar), librespot_gain(scalar), 1e-5)
            << "at scalar " << scalar;
    }
}

// Both ends are exact rather than left to the exponential, so silence is silence
// and full scale is bit transparent. librespot short-circuits the same points.
TEST(VolumeControlTest, endpoints_are_exact)
{
    EXPECT_EQ(volume_scalar_to_gain(0.0f), 0.0f);
    EXPECT_EQ(volume_scalar_to_gain(1.0f), 1.0f);

    EXPECT_FLOAT_EQ(volume_scalar_to_decibels(1.0f), 0.0f);
    EXPECT_FLOAT_EQ(volume_scalar_to_decibels(0.0f), -VolumeRangeDb);
}

// Readable checkpoints: these are the numbers a listener compares against the
// Spotify slider at the same percentage.
TEST(VolumeControlTest, known_positions)
{
    EXPECT_NEAR(volume_scalar_to_decibels(0.75f), -15.0f, 1e-3);
    EXPECT_NEAR(volume_scalar_to_decibels(0.50f), -30.0f, 1e-3);
    EXPECT_NEAR(volume_scalar_to_decibels(0.25f), -45.0f, 1e-3);
}

TEST(VolumeControlTest, gain_is_monotonic)
{
    float prev = -1.0f;

    for (int i = 0; i <= 100; i++) {
        const float gain = volume_scalar_to_gain(float(i) / 100.0f);

        EXPECT_GT(gain, prev) << "at scalar " << float(i) / 100.0f;
        prev = gain;
    }
}

TEST(VolumeControlTest, out_of_range_scalars_are_clamped)
{
    EXPECT_EQ(volume_scalar_to_gain(-0.5f), 0.0f);
    EXPECT_EQ(volume_scalar_to_gain(2.0f), 1.0f);

    EXPECT_FLOAT_EQ(volume_scalar_to_decibels(-0.5f), -VolumeRangeDb);
    EXPECT_FLOAT_EQ(volume_scalar_to_decibels(2.0f), 0.0f);
}

// The range is a parameter, so a caller can match a source configured
// differently; the default is the one shared with librespot and shairport-sync.
TEST(VolumeControlTest, range_is_configurable)
{
    EXPECT_FLOAT_EQ(volume_scalar_to_decibels(0.5f, 40.0f), -20.0f);
    EXPECT_NEAR(volume_scalar_to_gain(0.5f, 40.0f), librespot_gain(0.5, 40.0), 1e-5);

    EXPECT_FLOAT_EQ(VolumeRangeDb, 60.0f);
}
