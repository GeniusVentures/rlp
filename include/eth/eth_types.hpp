// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <rlp/intx.hpp>
#include <rlp/rlp_ethereum.hpp>

namespace eth {

using Hash256 = rlp::Hash256;
using Address = rlp::Address;
using Bloom = rlp::Bloom;

struct ForkId {
    std::array<uint8_t, 4> fork_hash{};
    uint64_t next_fork = 0;
};

struct StatusMessage {
    uint8_t protocol_version = 66;
    uint64_t network_id = 0;
    intx::uint256 total_difficulty{};
    Hash256 best_hash{};
    Hash256 genesis_hash{};
    ForkId fork_id{};
};

struct NewBlockHashEntry {
    Hash256 hash{};
    uint64_t number = 0;
};

struct NewBlockHashesMessage {
    std::vector<NewBlockHashEntry> entries;
};

struct NewPooledTransactionHashesMessage {
    std::vector<Hash256> hashes;
};

struct GetBlockHeadersMessage {
    std::optional<Hash256> start_hash;
    std::optional<uint64_t> start_number;
    uint64_t max_headers = 0;
    uint64_t skip = 0;
    bool reverse = false;
};

} // namespace eth

