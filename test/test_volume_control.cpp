/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "driver/volume_control.hpp"

#include <aspl/Context.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

using namespace rocvad;

namespace {

// Same span the driver uses, and the same one librespot and shairport-sync are
// configured with.
constexpr Float32 DbRange = -60.0f;

// librespot's LogMapping, transcribed from playback/src/mixer/mappings.rs:
//
//     let (db_ratio, ideal_factor) = coefficients(db_range);   // 10^(r/20), ln(that)
//     exp(ideal_factor * normalized_volume) / db_ratio
//
// This is the thing being matched, so the test compares against it directly
// rather than against a restatement of our own implementation.
double librespot_gain(double scalar)
{
    const double db_ratio = std::pow(10.0, -DbRange / 20.0);
    const double ideal_factor = std::log(db_ratio);

    return std::exp(ideal_factor * scalar) / db_ratio;
}

std::shared_ptr<LogVolumeControl> make_control(Float32 initial_scalar = 1.0f)
{
    aspl::VolumeControlParameters params;
    params.MinDecibelVolume = DbRange;
    params.MaxDecibelVolume = 0;

    return std::make_shared<LogVolumeControl>(
        std::make_shared<aspl::Context>(), params, initial_scalar);
}

} // namespace

// The mapping libASPL's default control fails to apply: scalar linear in dB.
TEST(VolumeControlTest, scalar_to_decibels_is_linear)
{
    auto control = make_control();

    for (int i = 0; i <= 100; i++) {
        const Float32 scalar = Float32(i) / 100.0f;

        EXPECT_NEAR(control->ConvertScalarToDecibels(scalar), -60.0f * (1.0f - scalar), 1e-3)
            << "at scalar " << scalar;
    }
}

TEST(VolumeControlTest, gain_matches_librespot)
{
    auto control = make_control();

    for (int i = 1; i < 100; i++) {
        const Float32 scalar = Float32(i) / 100.0f;

        EXPECT_NEAR(control->gain_for_scalar(scalar), librespot_gain(scalar), 1e-5)
            << "at scalar " << scalar;
    }
}

// Both ends are special-cased so silence is silence and full scale is bit-exact,
// matching how librespot short-circuits the same two points.
TEST(VolumeControlTest, endpoints_are_exact)
{
    auto control = make_control();

    EXPECT_EQ(control->gain_for_scalar(0.0f), 0.0f);
    EXPECT_EQ(control->gain_for_scalar(1.0f), 1.0f);

    EXPECT_FLOAT_EQ(control->ConvertScalarToDecibels(1.0f), 0.0f);
    EXPECT_FLOAT_EQ(control->ConvertScalarToDecibels(0.0f), DbRange);
}

// A few readable checkpoints: these are the numbers a user compares against the
// Spotify slider at the same percentage.
TEST(VolumeControlTest, known_positions)
{
    auto control = make_control();

    EXPECT_NEAR(control->ConvertScalarToDecibels(0.75f), -15.0f, 1e-3);
    EXPECT_NEAR(control->ConvertScalarToDecibels(0.50f), -30.0f, 1e-3);
    EXPECT_NEAR(control->ConvertScalarToDecibels(0.25f), -45.0f, 1e-3);
}

TEST(VolumeControlTest, decibels_round_trip)
{
    auto control = make_control();

    for (int i = 0; i <= 100; i++) {
        const Float32 scalar = Float32(i) / 100.0f;
        const Float32 db = control->ConvertScalarToDecibels(scalar);

        EXPECT_NEAR(control->ConvertDecibelsToScalar(db), scalar, 1e-4)
            << "at scalar " << scalar;
    }
}

// Storage is the base class's raw scale, which has 96 steps, so a set/get cycle
// is quantised rather than exact.
TEST(VolumeControlTest, scalar_survives_the_raw_scale)
{
    auto control = make_control();

    for (int i = 0; i <= 100; i += 5) {
        const Float32 scalar = Float32(i) / 100.0f;

        control->SetScalarValue(scalar);

        EXPECT_NEAR(control->GetScalarValue(), scalar, 1.0f / 96.0f)
            << "at scalar " << scalar;
    }
}

TEST(VolumeControlTest, apply_processing_scales_by_the_gain)
{
    auto control = make_control();

    control->SetScalarValue(0.5f);

    const Float32 expected = control->gain_for_scalar(control->GetScalarValue());

    std::vector<Float32> frames = {1.0f, -1.0f, 0.5f, -0.25f, 0.0f, 0.125f};
    const std::vector<Float32> input = frames;

    control->ApplyProcessing(frames.data(), UInt32(frames.size() / 2), 2);

    for (size_t i = 0; i < frames.size(); i++) {
        EXPECT_NEAR(frames[i], input[i] * expected, 1e-6) << "at sample " << i;
    }
}

TEST(VolumeControlTest, apply_processing_clamps)
{
    auto control = make_control();

    control->SetScalarValue(1.0f);

    std::vector<Float32> frames = {4.0f, -4.0f};

    control->ApplyProcessing(frames.data(), 1, 2);

    EXPECT_FLOAT_EQ(frames[0], 1.0f);
    EXPECT_FLOAT_EQ(frames[1], -1.0f);
}

// libASPL starts a control at full scale. On a device feeding an amplifier that
// is the wrong default, so the driver picks half the range instead.
TEST(VolumeControlTest, initial_volume_is_configurable)
{
    EXPECT_NEAR(make_control(0.5f)->GetScalarValue(), 0.5f, 1.0f / 96.0f);
    EXPECT_NEAR(make_control(1.0f)->GetScalarValue(), 1.0f, 1.0f / 96.0f);
    EXPECT_NEAR(make_control(0.0f)->GetScalarValue(), 0.0f, 1.0f / 96.0f);
}
