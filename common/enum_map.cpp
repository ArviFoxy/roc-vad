/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "enum_map.hpp"

namespace rocvad {

enum_map<rvpb::RvInterface, roc_interface> interface_map {
    {rvpb::RV_INTERFACE_CONSOLIDATED, ROC_INTERFACE_AGGREGATE, "consolidated"},
    {rvpb::RV_INTERFACE_AUDIO_SOURCE, ROC_INTERFACE_AUDIO_SOURCE, "audiosrc"},
    {rvpb::RV_INTERFACE_AUDIO_REPAIR, ROC_INTERFACE_AUDIO_REPAIR, "audiorpr"},
    {rvpb::RV_INTERFACE_AUDIO_CONTROL, ROC_INTERFACE_AUDIO_CONTROL, "audioctl"},
};

// PCM only: roc_format has a single member, ROC_FORMAT_PCM, and the sample
// representation lives in roc_subformat. Mapping to the subformat alone keeps
// the generic three-tuple enum_map usable here; callers pair it with
// ROC_FORMAT_PCM when filling roc_media_encoding.
//
// Names match roc's own subformat names (roc_audio/pcm_subformat.h), so
// "--packet-encoding-format s24" lines up with the "pcm@s24/48000/stereo"
// syntax roc uses elsewhere.
enum_map<rvpb::RvSampleFormat, roc_subformat> sample_format_map {
    // 8-bit
    {rvpb::ROC_SUBFORMAT_PCM_SINT8, ROC_SUBFORMAT_PCM_SINT8, "s8"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT8, ROC_SUBFORMAT_PCM_UINT8, "u8"},
    // 16-bit
    {rvpb::ROC_FORMAT_PCM_SINT16, ROC_SUBFORMAT_PCM_SINT16, "s16"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT16_LE, ROC_SUBFORMAT_PCM_SINT16_LE, "s16_le"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT16_BE, ROC_SUBFORMAT_PCM_SINT16_BE, "s16_be"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT16, ROC_SUBFORMAT_PCM_UINT16, "u16"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT16_LE, ROC_SUBFORMAT_PCM_UINT16_LE, "u16_le"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT16_BE, ROC_SUBFORMAT_PCM_UINT16_BE, "u16_be"},
    // 24-bit
    {rvpb::ROC_SUBFORMAT_PCM_SINT24, ROC_SUBFORMAT_PCM_SINT24, "s24"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT24_LE, ROC_SUBFORMAT_PCM_SINT24_LE, "s24_le"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT24_BE, ROC_SUBFORMAT_PCM_SINT24_BE, "s24_be"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT24, ROC_SUBFORMAT_PCM_UINT24, "u24"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT24_LE, ROC_SUBFORMAT_PCM_UINT24_LE, "u24_le"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT24_BE, ROC_SUBFORMAT_PCM_UINT24_BE, "u24_be"},
    // 32-bit integer
    {rvpb::ROC_SUBFORMAT_PCM_SINT32, ROC_SUBFORMAT_PCM_SINT32, "s32"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT32_LE, ROC_SUBFORMAT_PCM_SINT32_LE, "s32_le"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT32_BE, ROC_SUBFORMAT_PCM_SINT32_BE, "s32_be"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT32, ROC_SUBFORMAT_PCM_UINT32, "u32"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT32_LE, ROC_SUBFORMAT_PCM_UINT32_LE, "u32_le"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT32_BE, ROC_SUBFORMAT_PCM_UINT32_BE, "u32_be"},
    // 64-bit integer
    {rvpb::ROC_SUBFORMAT_PCM_SINT64, ROC_SUBFORMAT_PCM_SINT64, "s64"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT64_LE, ROC_SUBFORMAT_PCM_SINT64_LE, "s64_le"},
    {rvpb::ROC_SUBFORMAT_PCM_SINT64_BE, ROC_SUBFORMAT_PCM_SINT64_BE, "s64_be"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT64, ROC_SUBFORMAT_PCM_UINT64, "u64"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT64_LE, ROC_SUBFORMAT_PCM_UINT64_LE, "u64_le"},
    {rvpb::ROC_SUBFORMAT_PCM_UINT64_BE, ROC_SUBFORMAT_PCM_UINT64_BE, "u64_be"},
    // floating point
    {rvpb::ROC_SUBFORMAT_PCM_FLOAT32, ROC_SUBFORMAT_PCM_FLOAT32, "f32"},
    {rvpb::ROC_SUBFORMAT_PCM_FLOAT32_LE, ROC_SUBFORMAT_PCM_FLOAT32_LE, "f32_le"},
    {rvpb::ROC_SUBFORMAT_PCM_FLOAT32_BE, ROC_SUBFORMAT_PCM_FLOAT32_BE, "f32_be"},
    {rvpb::ROC_SUBFORMAT_PCM_FLOAT64, ROC_SUBFORMAT_PCM_FLOAT64, "f64"},
    {rvpb::ROC_SUBFORMAT_PCM_FLOAT64_LE, ROC_SUBFORMAT_PCM_FLOAT64_LE, "f64_le"},
    {rvpb::ROC_SUBFORMAT_PCM_FLOAT64_BE, ROC_SUBFORMAT_PCM_FLOAT64_BE, "f64_be"},
};

enum_map<rvpb::RvChannelLayout, roc_channel_layout> channel_layout_map {
    {rvpb::RV_CHANNEL_LAYOUT_MULTITRACK, ROC_CHANNEL_LAYOUT_MULTITRACK, "multitrack"},
    {rvpb::RV_CHANNEL_LAYOUT_MONO, ROC_CHANNEL_LAYOUT_MONO, "mono"},
    {rvpb::RV_CHANNEL_LAYOUT_STEREO, ROC_CHANNEL_LAYOUT_STEREO, "stereo"},
};

enum_map<rvpb::RvFecEncoding, roc_fec_encoding> fec_encoding_map {
    {rvpb::RV_FEC_ENCODING_DISABLE, ROC_FEC_ENCODING_DISABLE, "disable"},
    {rvpb::RV_FEC_ENCODING_DEFAULT, ROC_FEC_ENCODING_DEFAULT, "default"},
    {rvpb::RV_FEC_ENCODING_RS8M, ROC_FEC_ENCODING_RS8M, "rs8m"},
    {rvpb::RV_FEC_ENCODING_LDPC_STAIRCASE, ROC_FEC_ENCODING_LDPC_STAIRCASE, "ldpc"},
};

enum_map<rvpb::RvLatencyTunerBackend, roc_latency_tuner_backend>
    latency_tuner_backend_map {
        {rvpb::RV_LATENCY_TUNER_BACKEND_DEFAULT,
            ROC_LATENCY_TUNER_BACKEND_DEFAULT,
            "default"},
        {rvpb::RV_LATENCY_TUNER_BACKEND_NIQ, ROC_LATENCY_TUNER_BACKEND_NIQ, "niq"},
    };

enum_map<rvpb::RvLatencyTunerProfile, roc_latency_tuner_profile>
    latency_tuner_profile_map {
        {rvpb::RV_LATENCY_TUNER_PROFILE_DEFAULT,
            ROC_LATENCY_TUNER_PROFILE_DEFAULT,
            "default"},
        {rvpb::RV_LATENCY_TUNER_PROFILE_INTACT,
            ROC_LATENCY_TUNER_PROFILE_INTACT,
            "intact"},
        {rvpb::RV_LATENCY_TUNER_PROFILE_RESPONSIVE,
            ROC_LATENCY_TUNER_PROFILE_RESPONSIVE,
            "responsive"},
        {rvpb::RV_LATENCY_TUNER_PROFILE_GRADUAL,
            ROC_LATENCY_TUNER_PROFILE_GRADUAL,
            "gradual"},
        {rvpb::RV_LATENCY_TUNER_PROFILE_SECOND_ORDER,
            ROC_LATENCY_TUNER_PROFILE_SECOND_ORDER,
            "second-order"},
    };

enum_map<rvpb::RvResamplerBackend, roc_resampler_backend> resampler_backend_map {
    {rvpb::RV_RESAMPLER_BACKEND_DEFAULT, ROC_RESAMPLER_BACKEND_DEFAULT, "default"},
    {rvpb::RV_RESAMPLER_BACKEND_BUILTIN, ROC_RESAMPLER_BACKEND_BUILTIN, "builtin"},
    {rvpb::RV_RESAMPLER_BACKEND_SPEEX, ROC_RESAMPLER_BACKEND_SPEEX, "speex"},
    {rvpb::RV_RESAMPLER_BACKEND_SPEEXDEC, ROC_RESAMPLER_BACKEND_SPEEXDEC, "speexdec"},
};

enum_map<rvpb::RvResamplerProfile, roc_resampler_profile> resampler_profile_map {
    {rvpb::RV_RESAMPLER_PROFILE_DEFAULT, ROC_RESAMPLER_PROFILE_DEFAULT, "default"},
    {rvpb::RV_RESAMPLER_PROFILE_HIGH, ROC_RESAMPLER_PROFILE_HIGH, "high"},
    {rvpb::RV_RESAMPLER_PROFILE_MEDIUM, ROC_RESAMPLER_PROFILE_MEDIUM, "medium"},
    {rvpb::RV_RESAMPLER_PROFILE_LOW, ROC_RESAMPLER_PROFILE_LOW, "low"},
};

} // namespace rocvad
