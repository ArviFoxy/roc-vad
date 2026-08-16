/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aspl/VolumeControl.hpp>

#include <memory>

namespace rocvad {

// Volume control with a logarithmic taper.
//
// libASPL's default control multiplies samples by the scalar itself, which is a
// linear-amplitude taper: half the slider is -6 dB and a tenth of it is -20 dB,
// so the entire useful range sits in the bottom of the travel. It also computes
// a separate curve for the decibel values it reports to the HAL, so the mapping
// it advertises and the one it applies disagree.
//
// This maps the scalar linearly onto the decibel range instead, which is what
// librespot does for the Spotify stream this driver plays alongside (its
// LogMapping over a 60 dB range works out to dB = 60 * (scalar - 1)). Given the
// same range, the two sliders then behave identically at the same percentage.
//
// The decibel range comes from VolumeControlParameters, so it is chosen where
// the control is constructed rather than fixed here.
class LogVolumeControl : public aspl::VolumeControl
{
public:
    // initial_scalar is where a brand-new device starts. libASPL would begin at
    // MaxRawVolume, i.e. full scale, which is a poor first impression for a
    // device wired to a power amplifier. Only applies the first time: for a
    // device it has seen before, CoreAudio restores its own stored volume.
    LogVolumeControl(std::shared_ptr<const aspl::Context> context,
        const aspl::VolumeControlParameters& params,
        Float32 initial_scalar);

    // Linear in decibels, unlike the base class, which applies a power curve
    // between scalar and raw and then reports decibels derived from it.
    Float32 GetScalarValue() const override;
    OSStatus SetScalarValue(Float32 value) override;

    Float32 ConvertScalarToDecibels(Float32 value) const override;
    Float32 ConvertDecibelsToScalar(Float32 value) const override;

    // Invoked on the realtime thread by Stream::ApplyProcessing().
    void ApplyProcessing(Float32* frames,
        UInt32 frameCount,
        UInt32 channelCount) const override;

    // Gain this control would apply at the given scalar. Exposed so tests can
    // check the curve without a HAL device.
    Float32 gain_for_scalar(Float32 scalar) const;

private:
    const SInt32 min_raw_;
    const SInt32 max_raw_;
    const Float32 min_db_;
    const Float32 max_db_;
};

} // namespace rocvad
