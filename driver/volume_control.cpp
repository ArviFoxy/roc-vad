/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "volume_control.hpp"

#include <algorithm>
#include <cmath>

namespace rocvad {

LogVolumeControl::LogVolumeControl(std::shared_ptr<const aspl::Context> context,
    const aspl::VolumeControlParameters& params,
    Float32 initial_scalar)
    : aspl::VolumeControl(std::move(context), params)
    , min_raw_(params.MinRawVolume)
    , max_raw_(params.MaxRawVolume)
    , min_db_(params.MinDecibelVolume)
    , max_db_(params.MaxDecibelVolume)
{
    // Not SetRawValue(): that one notifies the HAL a property changed, and there
    // is nothing listening yet during construction. SetRawValueImpl() is the
    // plain assignment underneath it. It is virtual but not overridden here, so
    // calling it from the constructor resolves to the base implementation, which
    // is the one wanted.
    initial_scalar = std::clamp(initial_scalar, 0.0f, 1.0f);

    SetRawValueImpl(
        min_raw_ + SInt32(std::lround(initial_scalar * Float32(max_raw_ - min_raw_))));
}

Float32 LogVolumeControl::GetScalarValue() const
{
    const SInt32 range = max_raw_ - min_raw_;
    if (range <= 0) {
        return 1.0f;
    }

    return Float32(GetRawValue() - min_raw_) / Float32(range);
}

OSStatus LogVolumeControl::SetScalarValue(Float32 value)
{
    value = std::clamp(value, 0.0f, 1.0f);

    const SInt32 range = max_raw_ - min_raw_;

    // SetRawValue() is not virtual: it takes the write lock and notifies the HAL
    // that both the scalar and decibel properties changed, which is wanted here
    // too. Only the conversion differs from the base class.
    return SetRawValue(min_raw_ + SInt32(std::lround(value * Float32(range))));
}

Float32 LogVolumeControl::ConvertScalarToDecibels(Float32 value) const
{
    value = std::clamp(value, 0.0f, 1.0f);

    return min_db_ + value * (max_db_ - min_db_);
}

Float32 LogVolumeControl::ConvertDecibelsToScalar(Float32 value) const
{
    value = std::clamp(value, min_db_, max_db_);

    const Float32 range = max_db_ - min_db_;
    if (range <= 0.0f) {
        return 1.0f;
    }

    return (value - min_db_) / range;
}

Float32 LogVolumeControl::gain_for_scalar(Float32 scalar) const
{
    // Both ends are special-cased rather than left to the exponential, so that
    // zero is silence and full scale is bit-exact rather than 0.9999. librespot
    // short-circuits the same two points in its own mapping.
    if (scalar <= 0.0f) {
        return 0.0f;
    }
    if (scalar >= 1.0f) {
        return 1.0f;
    }

    return std::pow(10.0f, ConvertScalarToDecibels(scalar) / 20.0f);
}

void LogVolumeControl::ApplyProcessing(Float32* frames,
    UInt32 frameCount,
    UInt32 channelCount) const
{
    // Once per buffer, not per sample: this runs on the realtime thread, and it
    // allocates nothing and takes no locks (the raw value is an atomic).
    const Float32 gain = gain_for_scalar(GetScalarValue());

    for (UInt32 i = 0; i < frameCount * channelCount; i++) {
        frames[i] = std::clamp(frames[i] * gain, -1.0f, 1.0f);
    }
}

} // namespace rocvad
