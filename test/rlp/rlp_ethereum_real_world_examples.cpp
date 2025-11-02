#include <gtest/gtest.h>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <rlp/rlp_streaming.hpp>
#include <rlp/intx.hpp>
#include <array>
#include <string>
#include <iostream>

using namespace rlp;
using uint256_t = intx::uint256;

// ============================================================================
// Real-World Ethereum RLP Use Cases
// ============================================================================

// Use Case 1: Legacy Ethereum Transaction (Pre-EIP-155)
// Structure: [nonce, gasPrice, gasLimit, to, value, data, v, r, s]
TEST(RLPEthereumRealWorld, LegacyTransaction) {
    // Example: Send 1 ETH from Alice to Bob
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Nonce: 9
    EXPECT_TRUE(encoder.add(uint64_t(9)));
    
    // Gas Price: 20 Gwei
    uint256_t gas_price = intx::from_string<uint256_t>("20000000000");
    EXPECT_TRUE(encoder.add(gas_price));
    
    // Gas Limit: 21000
    EXPECT_TRUE(encoder.add(uint64_t(21000)));
    
    // To address: 20 bytes
    std::array<uint8_t, 20> to_address{};
    std::fill(to_address.begin(), to_address.end(), 0x35);
    EXPECT_TRUE(encoder.add(ByteView(to_address.data(), to_address.size())));
    
    // Value: 1 ETH
    uint256_t value = intx::from_string<uint256_t>("1000000000000000000");
    EXPECT_TRUE(encoder.add(value));
    
    // Data: empty
    EXPECT_TRUE(encoder.add(ByteView(nullptr, 0)));
    
    // v, r, s: signature components (32 bytes each for r and s)
    EXPECT_TRUE(encoder.add(uint8_t(27)));
    
    std::array<uint8_t, 32> r{};
    for (size_t i = 0; i < 32; ++i) r[i] = static_cast<uint8_t>(i);
    EXPECT_TRUE(encoder.add(ByteView(r.data(), r.size())));
    
    std::array<uint8_t, 32> s{};
    for (size_t i = 0; i < 32; ++i) s[i] = static_cast<uint8_t>(255 - i);
    EXPECT_TRUE(encoder.add(ByteView(s.data(), s.size())));
    
    EXPECT_TRUE(encoder.EndList());
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Legacy Transaction Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 2: EIP-1559 Transaction (Type 2) - Modern Fee Market
TEST(RLPEthereumRealWorld, EIP1559Transaction) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Chain ID
    EXPECT_TRUE(encoder.add(uint64_t(1))); // Ethereum mainnet
    
    // Nonce
    EXPECT_TRUE(encoder.add(uint64_t(42)));
    
    // Max Priority Fee Per Gas: 2 Gwei
    uint256_t max_priority_fee = intx::from_string<uint256_t>("2000000000");
    EXPECT_TRUE(encoder.add(max_priority_fee));
    
    // Max Fee Per Gas: 30 Gwei
    uint256_t max_fee = intx::from_string<uint256_t>("30000000000");
    EXPECT_TRUE(encoder.add(max_fee));
    
    // Gas Limit
    EXPECT_TRUE(encoder.add(uint64_t(21000)));
    
    // To address
    std::array<uint8_t, 20> to{};
    std::fill(to.begin(), to.end(), 0xAB);
    EXPECT_TRUE(encoder.add(ByteView(to.data(), to.size())));
    
    // Value: 0.5 ETH
    uint256_t value = intx::from_string<uint256_t>("500000000000000000");
    EXPECT_TRUE(encoder.add(value));
    
    // Data: empty
    EXPECT_TRUE(encoder.add(ByteView(nullptr, 0)));
    
    // Access List: empty list
    ASSERT_TRUE(encoder.BeginList());
    EXPECT_TRUE(encoder.EndList());
    
    EXPECT_TRUE(encoder.EndList());
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "EIP-1559 Transaction Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 3: Ethereum Block Header (15 fields)
TEST(RLPEthereumRealWorld, BlockHeader) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Parent Hash (32 bytes)
    std::array<uint8_t, 32> parent_hash{};
    for (size_t i = 0; i < 32; ++i) parent_hash[i] = static_cast<uint8_t>(i * 3);
    EXPECT_TRUE(encoder.add(ByteView(parent_hash.data(), parent_hash.size())));
    
    // Uncle Hash (32 bytes)
    std::array<uint8_t, 32> uncle_hash{};
    std::fill(uncle_hash.begin(), uncle_hash.end(), 0x11);
    EXPECT_TRUE(encoder.add(ByteView(uncle_hash.data(), uncle_hash.size())));
    
    // Coinbase (20 bytes) - miner address
    std::array<uint8_t, 20> coinbase{};
    std::fill(coinbase.begin(), coinbase.end(), 0xAA);
    EXPECT_TRUE(encoder.add(ByteView(coinbase.data(), coinbase.size())));
    
    // State Root, Transactions Root, Receipts Root (32 bytes each)
    for (int j = 0; j < 3; ++j) {
        std::array<uint8_t, 32> root{};
        for (size_t i = 0; i < 32; ++i) root[i] = static_cast<uint8_t>((j + 1) * i);
        EXPECT_TRUE(encoder.add(ByteView(root.data(), root.size())));
    }
    
    // Logs Bloom (256 bytes)
    std::vector<uint8_t> logs_bloom(256, 0);
    EXPECT_TRUE(encoder.add(ByteView(logs_bloom.data(), logs_bloom.size())));
    
    // Difficulty
    uint256_t difficulty = intx::from_string<uint256_t>("2000000000000000");
    EXPECT_TRUE(encoder.add(difficulty));
    
    // Number (block number)
    EXPECT_TRUE(encoder.add(uint64_t(15000000)));
    
    // Gas Limit
    EXPECT_TRUE(encoder.add(uint64_t(30000000)));
    
    // Gas Used
    EXPECT_TRUE(encoder.add(uint64_t(15500000)));
    
    // Timestamp
    EXPECT_TRUE(encoder.add(uint64_t(1699000000)));
    
    // Extra Data
    std::vector<uint8_t> extra_data{'G', 'e', 't', 'h'};
    EXPECT_TRUE(encoder.add(ByteView(extra_data.data(), extra_data.size())));
    
    // Mix Hash (32 bytes)
    std::array<uint8_t, 32> mix_hash{};
    EXPECT_TRUE(encoder.add(ByteView(mix_hash.data(), mix_hash.size())));
    
    // Nonce (8 bytes)
    EXPECT_TRUE(encoder.add(uint64_t(0x123456789ABCDEF0)));
    
    EXPECT_TRUE(encoder.EndList());
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Block Header Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 4: Account State (for Merkle Patricia Tree)
TEST(RLPEthereumRealWorld, AccountState) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Nonce
    EXPECT_TRUE(encoder.add(uint64_t(157)));
    
    // Balance: 50 ETH
    uint256_t balance = intx::from_string<uint256_t>("50000000000000000000");
    EXPECT_TRUE(encoder.add(balance));
    
    // Storage Root: root hash of the account's storage trie
    std::array<uint8_t, 32> storage_root{};
    for (size_t i = 0; i < 32; ++i) storage_root[i] = static_cast<uint8_t>(i * 7);
    EXPECT_TRUE(encoder.add(ByteView(storage_root.data(), storage_root.size())));
    
    // Code Hash: hash of the account's EVM bytecode
    std::array<uint8_t, 32> code_hash{};
    for (size_t i = 0; i < 32; ++i) code_hash[i] = static_cast<uint8_t>(255 - i * 3);
    EXPECT_TRUE(encoder.add(ByteView(code_hash.data(), code_hash.size())));
    
    EXPECT_TRUE(encoder.EndList());
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Account State Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 5: Streaming Large Contract Bytecode (up to 24KB)
TEST(RLPEthereumRealWorld, StreamingLargeContractBytecode) {
    // Ethereum max contract size is 24576 bytes (24KB)
    const size_t bytecode_size = 24576;
    
    RlpEncoder main_encoder;
    
    // Use streaming API for large bytecode
    auto result = RlpLargeStringEncoder::create(main_encoder);
    ASSERT_TRUE(result);
    auto streaming_encoder = std::move(result.value());
    
    // Simulate large bytecode in chunks
    const size_t chunk_size = 1024;
    std::vector<uint8_t> chunk(chunk_size);
    
    for (size_t offset = 0; offset < bytecode_size; offset += chunk_size) {
        // Fill chunk with bytecode pattern
        for (size_t i = 0; i < chunk_size; ++i) {
            chunk[i] = static_cast<uint8_t>((offset + i) % 256);
        }
        
        auto add_result = streaming_encoder.addChunk(ByteView(chunk.data(), chunk_size));
        EXPECT_TRUE(add_result);
    }
    
    auto finish_result = streaming_encoder.finish();
    EXPECT_TRUE(finish_result);
    
    auto encoded = main_encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Large Contract Bytecode RLP Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 6: Batch Transaction Encoding
TEST(RLPEthereumRealWorld, BatchTransactionEncoding) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList()); // Outer list for batch
    
    // Transaction 1
    ASSERT_TRUE(encoder.BeginList());
    EXPECT_TRUE(encoder.add(uint64_t(10)));
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("20000000000"))); // gasPrice
    EXPECT_TRUE(encoder.add(uint64_t(21000))); // gasLimit
    std::array<uint8_t, 20> to1{};
    std::fill(to1.begin(), to1.end(), 0x11);
    EXPECT_TRUE(encoder.add(ByteView(to1.data(), to1.size())));
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("1000000000000000000"))); // 1 ETH
    EXPECT_TRUE(encoder.add(ByteView(nullptr, 0))); // empty data
    EXPECT_TRUE(encoder.EndList());
    
    // Transaction 2
    ASSERT_TRUE(encoder.BeginList());
    EXPECT_TRUE(encoder.add(uint64_t(11)));
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("25000000000"))); // gasPrice
    EXPECT_TRUE(encoder.add(uint64_t(21000))); // gasLimit
    std::array<uint8_t, 20> to2{};
    std::fill(to2.begin(), to2.end(), 0x22);
    EXPECT_TRUE(encoder.add(ByteView(to2.data(), to2.size())));
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("500000000000000000"))); // 0.5 ETH
    EXPECT_TRUE(encoder.add(ByteView(nullptr, 0))); // empty data
    EXPECT_TRUE(encoder.EndList());
    
    // Transaction 3
    ASSERT_TRUE(encoder.BeginList());
    EXPECT_TRUE(encoder.add(uint64_t(12)));
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("30000000000"))); // gasPrice
    EXPECT_TRUE(encoder.add(uint64_t(21000))); // gasLimit
    std::array<uint8_t, 20> to3{};
    std::fill(to3.begin(), to3.end(), 0x33);
    EXPECT_TRUE(encoder.add(ByteView(to3.data(), to3.size())));
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("2000000000000000000"))); // 2 ETH
    EXPECT_TRUE(encoder.add(ByteView(nullptr, 0))); // empty data
    EXPECT_TRUE(encoder.EndList());
    
    EXPECT_TRUE(encoder.EndList()); // Close batch list
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Batch of 3 transactions, total size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 7: Simple Round-Trip Example
TEST(RLPEthereumRealWorld, SimpleRoundTrip) {
    // Encode a simple transaction-like structure
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    uint64_t nonce = 100;
    uint256_t gas_price = intx::from_string<uint256_t>("25000000000");
    uint64_t gas_limit = 21000;
    
    EXPECT_TRUE(encoder.add(nonce));
    EXPECT_TRUE(encoder.add(gas_price));
    EXPECT_TRUE(encoder.add(gas_limit));
    
    EXPECT_TRUE(encoder.EndList());
    
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    
    // Decode
    RlpDecoder decoder(ByteView(*encoded_result.value()));
    
    auto is_list = decoder.IsList();
    ASSERT_TRUE(is_list);
    EXPECT_TRUE(is_list.value());
    
    auto list_size_result = decoder.ReadListHeaderBytes();
    ASSERT_TRUE(list_size_result);
    
    uint64_t decoded_nonce;
    EXPECT_TRUE(decoder.read(decoded_nonce));
    EXPECT_EQ(nonce, decoded_nonce);
    
    uint256_t decoded_gas_price;
    EXPECT_TRUE(decoder.read(decoded_gas_price));
    EXPECT_EQ(gas_price, decoded_gas_price);
    
    uint64_t decoded_gas_limit;
    EXPECT_TRUE(decoder.read(decoded_gas_limit));
    EXPECT_EQ(gas_limit, decoded_gas_limit);
    
    EXPECT_TRUE(decoder.IsFinished());
    
    std::cout << "Simple Round-Trip: Encoded " << encoded_result.value()->size() << " bytes\n";
}

// Use Case 8: Transaction Receipt with Logs
TEST(RLPEthereumRealWorld, TransactionReceipt) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Status (1 = success)
    EXPECT_TRUE(encoder.add(uint8_t(1)));
    
    // Cumulative Gas Used
    EXPECT_TRUE(encoder.add(uint64_t(84000)));
    
    // Logs Bloom (256 bytes)
    std::vector<uint8_t> logs_bloom(256, 0);
    logs_bloom[50] = 0xFF; // Some bloom filter bits set
    EXPECT_TRUE(encoder.add(ByteView(logs_bloom.data(), logs_bloom.size())));
    
    // Logs array
    ASSERT_TRUE(encoder.BeginList());
    
    // Log 1: Transfer event
    ASSERT_TRUE(encoder.BeginList());
    // Address (20 bytes)
    std::array<uint8_t, 20> log_address{};
    std::fill(log_address.begin(), log_address.end(), 0xDD);
    EXPECT_TRUE(encoder.add(ByteView(log_address.data(), log_address.size())));
    
    // Topics (indexed event parameters)
    ASSERT_TRUE(encoder.BeginList());
    // Topic 0: Transfer event signature
    std::array<uint8_t, 32> topic0{};
    for (size_t i = 0; i < 32; ++i) topic0[i] = static_cast<uint8_t>(i);
    EXPECT_TRUE(encoder.add(ByteView(topic0.data(), topic0.size())));
    EXPECT_TRUE(encoder.EndList()); // topics
    
    // Data (non-indexed parameters)
    uint256_t transfer_amount = intx::from_string<uint256_t>("1000000000000000000");
    // In real logs, this would be ABI-encoded
    std::vector<uint8_t> data(32, 0);
    EXPECT_TRUE(encoder.add(ByteView(data.data(), data.size())));
    
    EXPECT_TRUE(encoder.EndList()); // log 1
    EXPECT_TRUE(encoder.EndList()); // logs array
    
    EXPECT_TRUE(encoder.EndList()); // receipt
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Transaction Receipt Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 9: EIP-2930 Access List Transaction
TEST(RLPEthereumRealWorld, AccessListTransaction) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Chain ID
    EXPECT_TRUE(encoder.add(uint64_t(1)));
    
    // Nonce
    EXPECT_TRUE(encoder.add(uint64_t(77)));
    
    // Gas Price
    EXPECT_TRUE(encoder.add(intx::from_string<uint256_t>("100000000000")));
    
    // Gas Limit
    EXPECT_TRUE(encoder.add(uint64_t(100000)));
    
    // To
    std::array<uint8_t, 20> to{};
    std::fill(to.begin(), to.end(), 0xBB);
    EXPECT_TRUE(encoder.add(ByteView(to.data(), to.size())));
    
    // Value
    EXPECT_TRUE(encoder.add(uint64_t(0)));
    
    // Data: contract call
    std::vector<uint8_t> data(36, 0xAB); // Function selector + args
    EXPECT_TRUE(encoder.add(ByteView(data.data(), data.size())));
    
    // Access List: pre-declared storage access for gas optimization
    ASSERT_TRUE(encoder.BeginList());
    
    // Access List Entry 1
    ASSERT_TRUE(encoder.BeginList());
    // Address
    std::array<uint8_t, 20> access_addr{};
    std::fill(access_addr.begin(), access_addr.end(), 0xCC);
    EXPECT_TRUE(encoder.add(ByteView(access_addr.data(), access_addr.size())));
    
    // Storage Keys
    ASSERT_TRUE(encoder.BeginList());
    std::array<uint8_t, 32> storage_key{};
    storage_key[31] = 0x05; // Slot 5
    EXPECT_TRUE(encoder.add(ByteView(storage_key.data(), storage_key.size())));
    EXPECT_TRUE(encoder.EndList()); // storage keys
    
    EXPECT_TRUE(encoder.EndList()); // access list entry
    EXPECT_TRUE(encoder.EndList()); // access list
    
    EXPECT_TRUE(encoder.EndList()); // transaction
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Access List Transaction Size: " << encoded.value()->size() << " bytes\n";
}

// Use Case 10: Contract Creation Transaction
TEST(RLPEthereumRealWorld, ContractCreationTransaction) {
    RlpEncoder encoder;
    ASSERT_TRUE(encoder.BeginList());
    
    // Nonce
    EXPECT_TRUE(encoder.add(uint64_t(0)));
    
    // Gas Price: 50 Gwei
    uint256_t gas_price = intx::from_string<uint256_t>("50000000000");
    EXPECT_TRUE(encoder.add(gas_price));
    
    // Gas Limit: contracts need more gas
    EXPECT_TRUE(encoder.add(uint64_t(3000000)));
    
    // To: empty (indicates contract creation)
    EXPECT_TRUE(encoder.add(ByteView(nullptr, 0)));
    
    // Value: 0 (or could include initial endowment)
    EXPECT_TRUE(encoder.add(uint64_t(0)));
    
    // Data: contract bytecode (simplified - real contracts are larger)
    std::vector<uint8_t> contract_code(500, 0x60); // Example bytecode
    contract_code[0] = 0x60; // PUSH1
    contract_code[1] = 0x80; // 0x80
    contract_code[2] = 0x60; // PUSH1
    contract_code[3] = 0x40; // 0x40
    EXPECT_TRUE(encoder.add(ByteView(contract_code.data(), contract_code.size())));
    
    // v, r, s: signature (simplified)
    EXPECT_TRUE(encoder.add(uint8_t(28)));
    std::array<uint8_t, 32> r{}, s{};
    EXPECT_TRUE(encoder.add(ByteView(r.data(), r.size())));
    EXPECT_TRUE(encoder.add(ByteView(s.data(), s.size())));
    
    EXPECT_TRUE(encoder.EndList());
    
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    std::cout << "Contract Creation Transaction Size: " << encoded.value()->size() << " bytes\n";
}
