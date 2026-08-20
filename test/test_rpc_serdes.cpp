/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include "driver/device_defs.hpp"
#include "driver/rpc_serdes.hpp"

using namespace rocvad;

// Sender slot configs survive a to_rpc / from_rpc round trip.
TEST(RpcSerdesTest, sender_slots_round_trip)
{
    DeviceInfo info;
    info.type = DeviceType::Sender;
    info.uid = "test-uid";
    info.name = "test-device";
    info.sender_config = DeviceSenderConfig {};

    DeviceSlotConfig slot_a;
    slot_a.slot = 0;
    slot_a.tracks = "0";
    slot_a.name = "leg_a";

    DeviceSlotConfig slot_b;
    slot_b.slot = 1;
    slot_b.tracks = "1-2";

    info.sender_config->slots = {slot_a, slot_b};

    rvpb::RvDeviceInfo rpc_info;
    device_info_to_rpc(rpc_info, info);

    DeviceInfo out_info;
    device_info_from_rpc(out_info, rpc_info);

    ASSERT_TRUE(out_info.sender_config.has_value());
    ASSERT_EQ(out_info.sender_config->slots.size(), 2);

    EXPECT_EQ(out_info.sender_config->slots[0].slot, 0);
    EXPECT_EQ(out_info.sender_config->slots[0].tracks, "0");
    EXPECT_EQ(out_info.sender_config->slots[0].name, "leg_a");

    EXPECT_EQ(out_info.sender_config->slots[1].slot, 1);
    EXPECT_EQ(out_info.sender_config->slots[1].tracks, "1-2");
    EXPECT_EQ(out_info.sender_config->slots[1].name, "");
}

// Duplicate slot entries are tolerated on load: the first entry wins and
// nothing throws (a throw here would drop the whole persisted device list).
TEST(RpcSerdesTest, sender_slots_duplicate_lenient)
{
    rvpb::RvDeviceInfo rpc_info;
    rpc_info.set_type(rvpb::RV_DEVICE_TYPE_SENDER);
    rpc_info.set_uid("test-uid");
    rpc_info.set_name("test-device");

    auto* slot_1 = rpc_info.mutable_sender_config()->add_slots();
    slot_1->set_slot(7);
    slot_1->set_tracks("0");
    slot_1->set_name("first");

    auto* slot_2 = rpc_info.mutable_sender_config()->add_slots();
    slot_2->set_slot(7);
    slot_2->set_tracks("1");
    slot_2->set_name("second");

    DeviceInfo out_info;
    device_info_from_rpc(out_info, rpc_info);

    ASSERT_TRUE(out_info.sender_config.has_value());
    ASSERT_EQ(out_info.sender_config->slots.size(), 1);
    EXPECT_EQ(out_info.sender_config->slots[0].slot, 7);
    EXPECT_EQ(out_info.sender_config->slots[0].name, "first");
}

// Endpoint slot indices are 64-bit end to end.
TEST(RpcSerdesTest, endpoint_slot_uint64)
{
    const uint64_t big_slot = (1ull << 40) + 123;

    DeviceEndpointInfo info;
    info.slot = big_slot;
    info.interface = ROC_INTERFACE_AUDIO_SOURCE;
    info.uri = "rtp://192.168.0.1:10001";

    rvpb::RvEndpointInfo rpc_info;
    endpoint_info_to_rpc(rpc_info, info);

    DeviceEndpointInfo out_info;
    endpoint_info_from_rpc(out_info, rpc_info);

    EXPECT_EQ(out_info.slot, big_slot);
    EXPECT_EQ(out_info.uri, info.uri);
}
