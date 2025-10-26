// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <rlpx/protocol/messages.hpp>
#include <vector>

using namespace rlpx;
using namespace rlpx::protocol;

TEST(ProtocolMessagesTest, HelloEncodeBasic) {
    HelloMessage msg;
    msg.protocol_version = 5;
    msg.client_id = "TestClient/v1.0";
    msg.listen_port = 30303;
    msg.node_id.fill(0x42);
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
}

TEST(ProtocolMessagesTest, HelloEncodeWithCapabilities) {
    HelloMessage msg;
    msg.protocol_version = 5;
    msg.client_id = "TestClient/v1.0";
    msg.capabilities = {
        {"eth", 66},
        {"snap", 1}
    };
    msg.listen_port = 30303;
    msg.node_id.fill(0x42);
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
}

TEST(ProtocolMessagesTest, HelloRoundtrip) {
    HelloMessage original;
    original.protocol_version = 5;
    original.client_id = "TestClient/v1.0";
    original.capabilities = {
        {"eth", 66},
        {"snap", 1},
        {"wit", 0}
    };
    original.listen_port = 30303;
    original.node_id.fill(0x42);
    
    auto encoded = original.encode();
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = HelloMessage::decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
    
    EXPECT_EQ(decoded.value().protocol_version, original.protocol_version);
    EXPECT_EQ(decoded.value().client_id, original.client_id);
    EXPECT_EQ(decoded.value().listen_port, original.listen_port);
    EXPECT_EQ(decoded.value().node_id, original.node_id);
    EXPECT_EQ(decoded.value().capabilities.size(), original.capabilities.size());
}

TEST(ProtocolMessagesTest, HelloEmptyClientId) {
    HelloMessage msg;
    msg.protocol_version = 5;
    msg.client_id = "";
    msg.listen_port = 30303;
    msg.node_id.fill(0x42);
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
}

TEST(ProtocolMessagesTest, DisconnectEncodeRequested) {
    DisconnectMessage msg;
    msg.reason = DisconnectReason::kRequested;
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
}

TEST(ProtocolMessagesTest, DisconnectEncodeClientQuitting) {
    DisconnectMessage msg;
    msg.reason = DisconnectReason::kClientQuitting;
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
}

TEST(ProtocolMessagesTest, DisconnectRoundtrip) {
    DisconnectMessage original;
    original.reason = DisconnectReason::kTooManyPeers;
    
    auto encoded = original.encode();
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = DisconnectMessage::decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
    
    EXPECT_EQ(decoded.value().reason, original.reason);
}

TEST(ProtocolMessagesTest, DisconnectAllReasons) {
    std::vector<DisconnectReason> reasons = {
        DisconnectReason::kRequested,
        DisconnectReason::kTcpError,
        DisconnectReason::kProtocolError,
        DisconnectReason::kUselessPeer,
        DisconnectReason::kTooManyPeers,
        DisconnectReason::kAlreadyConnected,
        DisconnectReason::kIncompatibleVersion,
        DisconnectReason::kInvalidIdentity,
        DisconnectReason::kClientQuitting,
        DisconnectReason::kUnexpectedIdentity,
        DisconnectReason::kSelfConnection,
        DisconnectReason::kTimeout,
        DisconnectReason::kSubprotocolError
    };
    
    for ( auto reason : reasons ) {
        DisconnectMessage msg;
        msg.reason = reason;
        
        auto encoded = msg.encode();
        ASSERT_TRUE(encoded.has_value()) << "Failed to encode reason: " << static_cast<int>(reason);
        
        auto decoded = DisconnectMessage::decode(encoded.value());
        ASSERT_TRUE(decoded.has_value()) << "Failed to decode reason: " << static_cast<int>(reason);
        
        EXPECT_EQ(decoded.value().reason, reason);
    }
}

TEST(ProtocolMessagesTest, PingEncode) {
    PingMessage msg;
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
}

TEST(ProtocolMessagesTest, PingRoundtrip) {
    PingMessage original;
    
    auto encoded = original.encode();
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = PingMessage::decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
}

TEST(ProtocolMessagesTest, PongEncode) {
    PongMessage msg;
    
    auto result = msg.encode();
    
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
}

TEST(ProtocolMessagesTest, PongRoundtrip) {
    PongMessage original;
    
    auto encoded = original.encode();
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = PongMessage::decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
}

TEST(ProtocolMessagesTest, MessageWrapper) {
    Message msg;
    msg.id = kHelloMessageId;
    msg.payload = {0x01, 0x02, 0x03};
    
    EXPECT_TRUE(msg.is_hello());
    EXPECT_FALSE(msg.is_disconnect());
    EXPECT_FALSE(msg.is_ping());
    EXPECT_FALSE(msg.is_pong());
}

TEST(ProtocolMessagesTest, MessageWrapperDisconnect) {
    Message msg;
    msg.id = kDisconnectMessageId;
    msg.payload = {0x01, 0x02, 0x03};
    
    EXPECT_FALSE(msg.is_hello());
    EXPECT_TRUE(msg.is_disconnect());
    EXPECT_FALSE(msg.is_ping());
    EXPECT_FALSE(msg.is_pong());
}

TEST(ProtocolMessagesTest, MessageWrapperPing) {
    Message msg;
    msg.id = kPingMessageId;
    
    EXPECT_FALSE(msg.is_hello());
    EXPECT_FALSE(msg.is_disconnect());
    EXPECT_TRUE(msg.is_ping());
    EXPECT_FALSE(msg.is_pong());
}

TEST(ProtocolMessagesTest, MessageWrapperPong) {
    Message msg;
    msg.id = kPongMessageId;
    
    EXPECT_FALSE(msg.is_hello());
    EXPECT_FALSE(msg.is_disconnect());
    EXPECT_FALSE(msg.is_ping());
    EXPECT_TRUE(msg.is_pong());
}

TEST(ProtocolMessagesTest, HelloLargeClientId) {
    HelloMessage msg;
    msg.protocol_version = 5;
    msg.client_id = std::string(1000, 'X');  // Very long client ID
    msg.listen_port = 30303;
    msg.node_id.fill(0x42);
    
    auto encoded = msg.encode();
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = HelloMessage::decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value().client_id.size(), 1000);
}

TEST(ProtocolMessagesTest, HelloManyCapabilities) {
    HelloMessage msg;
    msg.protocol_version = 5;
    msg.client_id = "TestClient";
    msg.listen_port = 30303;
    msg.node_id.fill(0x42);
    
    // Add many capabilities
    for ( int i = 0; i < 10; ++i ) {
        msg.capabilities.push_back({"proto" + std::to_string(i), static_cast<uint8_t>(i)});
    }
    
    auto encoded = msg.encode();
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = HelloMessage::decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value().capabilities.size(), 10);
}
