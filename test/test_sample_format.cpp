/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "common/enum_map.hpp"
#include "driver/rpc_serdes.hpp"
#include "tool/format.hpp"
#include "tool/parse.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>

using namespace rocvad;

struct SampleFormatTest : testing::Test
{
    // A packet encoding that is valid apart from the format under test.
    rvpb::RvPacketEncoding make_rpc_encoding(rvpb::RvSampleFormat format)
    {
        rvpb::RvPacketEncoding enc;
        enc.set_encoding_id(101);
        enc.set_sample_rate(48000);
        enc.set_sample_format(format);
        enc.set_channel_layout(rvpb::RV_CHANNEL_LAYOUT_STEREO);
        return enc;
    }
};

// Every entry survives rpc -> roc -> rpc unchanged. This is what catches an
// entry whose two enum values do not correspond, which is how the map was
// broken before: "s16" was paired with a float32 constant.
TEST_F(SampleFormatTest, rpc_round_trip)
{
    for (const auto& entry : sample_format_map) {
        const auto rpc_value = std::get<0>(entry);
        const auto roc_value = std::get<1>(entry);
        const auto name = std::get<2>(entry);

        DevicePacketEncoding decoded;
        packet_encoding_from_rpc(decoded, make_rpc_encoding(rpc_value));

        EXPECT_EQ(decoded.spec.subformat, roc_value) << "subformat mismatch for " << name;
        EXPECT_EQ(decoded.spec.format, ROC_FORMAT_PCM) << "format should be PCM for "
                                                       << name;

        rvpb::RvPacketEncoding reencoded;
        packet_encoding_to_rpc(reencoded, decoded);

        EXPECT_EQ(reencoded.sample_format(), rpc_value) << "round trip failed for " << name;
        EXPECT_EQ(reencoded.sample_rate(), 48000u);
    }
}

// Every entry is reachable by name from the command line, and prints back as
// the same name.
TEST_F(SampleFormatTest, parse_and_format_by_name)
{
    for (const auto& entry : sample_format_map) {
        const auto rpc_value = std::get<0>(entry);
        const auto name = std::get<2>(entry);

        rvpb::RvSampleFormat parsed = {};
        EXPECT_TRUE(parse_enum("--packet-encoding-format", sample_format_map, name, parsed))
            << "could not parse " << name;
        EXPECT_EQ(parsed, rpc_value) << "wrong value parsed for " << name;

        EXPECT_EQ(format_enum(sample_format_map, rpc_value), name);
    }
}

TEST_F(SampleFormatTest, names_are_unique)
{
    std::set<std::string> names;

    for (const auto& entry : sample_format_map) {
        const auto name = std::get<2>(entry);
        EXPECT_TRUE(names.insert(name).second) << "duplicate name " << name;
    }

    EXPECT_EQ(names.size(), sample_format_map.size());
}

TEST_F(SampleFormatTest, parse_rejects_unknown_name)
{
    rvpb::RvSampleFormat parsed = {};

    EXPECT_FALSE(parse_enum("--packet-encoding-format", sample_format_map, "s99", parsed));
    EXPECT_FALSE(parse_enum("--packet-encoding-format", sample_format_map, "", parsed));
    EXPECT_FALSE(
        parse_enum("--packet-encoding-format", sample_format_map, "pcm@s24", parsed));
}

TEST_F(SampleFormatTest, help_text_lists_every_name)
{
    const std::string supported = supported_enum_values(sample_format_map);

    for (const auto& entry : sample_format_map) {
        EXPECT_NE(supported.find(std::get<2>(entry)), std::string::npos)
            << std::get<2>(entry) << " missing from help text";
    }
}

// The specific mappings this change exists for. s24 is what the receivers on
// the audio server expect; f32 is what the previous code produced no matter
// which name was given.
TEST_F(SampleFormatTest, known_formats_map_to_expected_subformats)
{
    struct
    {
        const char* name;
        roc_subformat expected;
    } cases[] = {
        {"s16", ROC_SUBFORMAT_PCM_SINT16},
        {"s24", ROC_SUBFORMAT_PCM_SINT24},
        {"s32", ROC_SUBFORMAT_PCM_SINT32},
        {"f32", ROC_SUBFORMAT_PCM_FLOAT32},
        {"s24_le", ROC_SUBFORMAT_PCM_SINT24_LE},
        {"s24_be", ROC_SUBFORMAT_PCM_SINT24_BE},
    };

    for (const auto& c : cases) {
        rvpb::RvSampleFormat parsed = {};
        ASSERT_TRUE(
            parse_enum("--packet-encoding-format", sample_format_map, c.name, parsed))
            << c.name;

        DevicePacketEncoding decoded;
        packet_encoding_from_rpc(decoded, make_rpc_encoding(parsed));

        EXPECT_EQ(decoded.spec.subformat, c.expected) << c.name;
    }
}

// Clients built before the other formats existed send tag 0, which has always
// meant 16-bit signed.
TEST_F(SampleFormatTest, legacy_zero_tag_is_s16)
{
    DevicePacketEncoding decoded;
    packet_encoding_from_rpc(decoded, make_rpc_encoding(rvpb::ROC_FORMAT_PCM_SINT16));

    EXPECT_EQ(decoded.spec.subformat, ROC_SUBFORMAT_PCM_SINT16);
    EXPECT_EQ(format_enum(sample_format_map, rvpb::ROC_FORMAT_PCM_SINT16), "s16");
}
