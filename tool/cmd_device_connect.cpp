/*
 * Copyright (c) Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "cmd_device_connect.hpp"
#include "connector.hpp"
#include "format.hpp"
#include "parse.hpp"
#include "print.hpp"

#include <spdlog/spdlog.h>

using namespace rocvad;

CmdDeviceConnect::CmdDeviceConnect(CLI::App& parent)
{
    auto command =
        parent.add_subcommand("connect", "Connect virtual device to remote endpoint(s)");

    command->add_flag("-u,--uid", use_uid_, "Select device by UID instead of index");
    command->add_option("index", index_or_uid_, "Device index (or UID of --uid is used)")
        ->required()
        ->expected(1);

    command->add_option("--slot", slot_, "Select slot to which to connect endpoint(s)");

    command->add_option("--track",
        track_,
        "Input track(s) the slot sends, e.g. '0', '2', or '0-1'"
        " (multiroom; requires multitrack device encoding and --source)");
    command->add_option("--slot-name",
        slot_name_,
        "Slot name, attached as 'slot' label to exported metrics"
        " (requires --source)");

    command->add_option(
        "-s,--source", source_endpoint_, "Connect endpoint for audio source packets");
    command->add_option(
        "-r,--repair", repair_endpoint_, "Connect endpoint for audio repair packets");
    command->add_option(
        "-c,--control", control_endpoint_, "Connect endpoint for audio control packets");

    register_command(command);
}

bool CmdDeviceConnect::execute(const Environment& env)
{
    if (!use_uid_ && !parse_index(index_or_uid_, index_)) {
        return false;
    }

    if (!source_endpoint_ && !repair_endpoint_ && !control_endpoint_) {
        spdlog::error(
            "at least one of the options should be specified:"
            " --source, --repair, --control");
        return false;
    }

    if ((track_ || slot_name_) && !source_endpoint_) {
        spdlog::error("--track and --slot-name require --source"
                      " (slot parameters apply before the slot's first endpoint)");
        return false;
    }

    if (source_endpoint_ && source_endpoint_->empty()) {
        spdlog::error("--source endpoint is empty string");
        return false;
    }

    if (repair_endpoint_ && repair_endpoint_->empty()) {
        spdlog::error("--repair endpoint is empty string");
        return false;
    }

    if (control_endpoint_ && control_endpoint_->empty()) {
        spdlog::error("--control endpoint is empty string");
        return false;
    }

    Connector conn;

    auto stub = conn.connect();
    if (!stub) {
        return false;
    }

    if (source_endpoint_) {
        if (!send_command_(stub,
                "--source",
                rvpb::RV_INTERFACE_AUDIO_SOURCE,
                *source_endpoint_,
                true)) {
            return false;
        }
    }

    if (repair_endpoint_) {
        if (!send_command_(
                stub, "--repair", rvpb::RV_INTERFACE_AUDIO_REPAIR, *repair_endpoint_)) {
            return false;
        }
    }

    if (control_endpoint_) {
        if (!send_command_(stub,
                "--control",
                rvpb::RV_INTERFACE_AUDIO_CONTROL,
                *control_endpoint_)) {
            return false;
        }
    }

    return true;
}

bool CmdDeviceConnect::send_command_(rvpb::RvDriver::Stub* stub,
    const char* name,
    rvpb::RvInterface interface,
    const std::string& uri,
    bool attach_slot_config)
{
    spdlog::debug("sending connect command for {} endpoint", name);

    grpc::ClientContext context;
    rvpb::RvEndpointRequest request;
    rvpb::RvEndpointInfo response;

    if (use_uid_) {
        request.mutable_device()->set_uid(index_or_uid_);
    } else {
        request.mutable_device()->set_index(index_);
    }

    if (slot_) {
        request.mutable_endpoint()->set_slot(*slot_);
    }
    request.mutable_endpoint()->set_interface(interface);
    request.mutable_endpoint()->set_uri(uri);

    if (attach_slot_config && (track_ || slot_name_)) {
        auto* slot_config = request.mutable_slot_config();
        if (slot_) {
            slot_config->set_slot(*slot_);
        }
        if (track_) {
            slot_config->set_tracks(*track_);
        }
        if (slot_name_) {
            slot_config->set_name(*slot_name_);
        }
    }

    const grpc::Status status = stub->connect(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("failed to connect endpoint: {}", status.error_message());
        return false;
    }

    print_endpoint_info(response);

    return true;
}
