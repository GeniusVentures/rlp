#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <cstdint>
#include <cstddef>
#include <vector>

// Fuzz test for encoder - tests encoding of random data structures
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    
    rlp::RlpEncoder encoder;
    
    // Use first byte to determine operation
    uint8_t operation = data[0] % 10;
    const uint8_t* payload = data + 1;
    size_t payload_size = size - 1;
    
    switch (operation) {
        case 0: // Add raw bytes
            if (payload_size > 0) {
                encoder.add(rlp::ByteView(reinterpret_cast<const char*>(payload), payload_size));
            }
            break;
            
        case 1: // Add as uint8_t
            if (payload_size >= 1) {
                encoder.add(payload[0]);
            }
            break;
            
        case 2: // Add as uint16_t
            if (payload_size >= 2) {
                uint16_t val;
                std::memcpy(&val, payload, 2);
                encoder.add(val);
            }
            break;
            
        case 3: // Add as uint32_t
            if (payload_size >= 4) {
                uint32_t val;
                std::memcpy(&val, payload, 4);
                encoder.add(val);
            }
            break;
            
        case 4: // Add as uint64_t
            if (payload_size >= 8) {
                uint64_t val;
                std::memcpy(&val, payload, 8);
                encoder.add(val);
            }
            break;
            
        case 5: // Create a list with multiple elements
        {
            encoder.BeginList();
            size_t offset = 0;
            while (offset + 1 < payload_size) {
                uint8_t elem_size = payload[offset] % 32;
                offset++;
                if (offset + elem_size <= payload_size) {
                    encoder.add(rlp::ByteView(reinterpret_cast<const char*>(payload + offset), elem_size));
                    offset += elem_size;
                } else {
                    break;
                }
            }
            encoder.EndList();
            break;
        }
            
        case 6: // Nested lists
        {
            encoder.BeginList();
            encoder.BeginList();
            if (payload_size > 0) {
                encoder.add(payload[0]);
            }
            encoder.EndList();
            encoder.EndList();
            break;
        }
            
        case 7: // Add uint256
        {
            intx::uint256 val = 0;
            size_t copy_size = std::min(payload_size, size_t(32));
            if (copy_size > 0) {
                // Build uint256 from bytes
                for (size_t i = 0; i < copy_size; ++i) {
                    val = (val << 8) | payload[i];
                }
            }
            encoder.add(val);
            break;
        }
            
        case 8: // Test AddRaw
            if (payload_size > 0) {
                // First encode something normally
                rlp::RlpEncoder temp_encoder;
                temp_encoder.add(payload[0]);
                auto temp_result = temp_encoder.GetBytes();
                if (temp_result) {
                    // Then add it raw to main encoder
                    encoder.AddRaw(**temp_result);
                }
            }
            break;
            
        case 9: // Mix of operations
        {
            encoder.BeginList();
            if (payload_size >= 1) encoder.add(payload[0]);
            if (payload_size >= 2) encoder.add(uint16_t(payload[1]));
            encoder.BeginList();
            if (payload_size >= 3) encoder.add(payload[2]);
            encoder.EndList();
            encoder.EndList();
            break;
        }
    }
    
    // Try to get the encoded bytes
    auto result = encoder.GetBytes();
    
    // If encoding succeeded, try to decode for roundtrip verification
    if (result) {
        const rlp::Bytes& encoded = **result;
        rlp::RlpDecoder decoder(encoded);
        
        // Try various decode operations
        auto header = decoder.PeekHeader();
        auto is_list = decoder.IsList();
        auto is_string = decoder.IsString();
        
        // The actual decoding might fail for complex structures,
        // but that's okay - we're testing that it doesn't crash
    }
    
    return 0;
}
