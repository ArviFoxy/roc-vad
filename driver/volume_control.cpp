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

float volume_scalar_to_decibels(float scalar, float db_range)
{
    scalar = std::clamp(scalar, 0.0f, 1.0f);

    return db_range * (scalar - 1.0f);
}

float volume_scalar_to_gain(float scalar, float db_range)
{
    if (scalar <= 0.0f) {
        return 0.0f;
    }
    if (scalar >= 1.0f) {
        return 1.0f;
    }

    return std::pow(10.0f, volume_scalar_to_decibels(scalar, db_range) / 20.0f);
}

} // namespace rocvad
