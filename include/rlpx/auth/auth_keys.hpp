// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rlpx_types.hpp"

namespace rlpx::auth {

// Authentication key material generated during handshake
struct AuthKeyMaterial {
    PublicKey peer_public_key;
    PublicKey peer_ephemeral_public_key;
    PublicKey local_ephemeral_public_key;
    PrivateKey local_ephemeral_private_key;
    Nonce initiator_nonce;
    Nonce recipient_nonce;
    ByteBuffer initiator_auth_message;
    ByteBuffer recipient_ack_message;
};

// Derived frame encryption secrets
struct FrameSecrets {
    AesKey aes_secret;
    MacKey mac_secret;
    MacDigest egress_mac_seed;
    MacDigest ingress_mac_seed;
};

} // namespace rlpx::auth
