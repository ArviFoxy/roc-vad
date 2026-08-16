/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace rocvad {

// Span of the volume slider, in dB below unity.
//
// Matches librespot's --volume-range and shairport-sync's volume_range_db, the
// two other sources feeding the same sink on the server, so a given percentage
// means the same attenuation on all three.
constexpr float VolumeRangeDb = 60;

// Where a newly created device starts. libASPL would start at full scale, which
// is an unpleasant surprise on a device connected to a power amplifier.
constexpr float InitialVolumeScalar = 0.5f;

// Decibels for a CoreAudio volume scalar, linear across the range: 100% is
// 0 dB, 50% is -VolumeRangeDb/2, 0% is -VolumeRangeDb.
//
// This is librespot's mapping. Its LogMapping computes
// exp(ln(10^(r/20)) * v) / 10^(r/20), which is 10^(r*(v-1)/20), i.e. exactly
// linear in decibels.
float volume_scalar_to_decibels(float scalar, float db_range = VolumeRangeDb);

// Gain factor to multiply samples by for a given scalar.
//
// Both ends are exact rather than left to the exponential, so that zero is
// silence and full scale is bit transparent; librespot short-circuits the same
// two points.
float volume_scalar_to_gain(float scalar, float db_range = VolumeRangeDb);

} // namespace rocvad
