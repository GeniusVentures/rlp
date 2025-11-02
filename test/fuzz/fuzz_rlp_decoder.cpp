#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <cstdint>
#include <cstddef>
#include <vector>

// LibFuzzer interface
// See: https://llvm.org/docs/LibFuzzer.html
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    
    // Create a ByteView from the fuzz input
    rlp::ByteView input(reinterpret_cast<const char*>(data), size);
    
    // Test 1: Decode arbitrary data
    rlp::RlpDecoder decoder(input);
    
    // Try to peek header
    auto header_result = decoder.PeekHeader();
    if (!header_result) {
        return 0; // Invalid input, which is fine
    }
    
    // Try to determine if it's a list or string
    auto is_list_result = decoder.IsList();
    auto is_string_result = decoder.IsString();
    
    // Try to read as bytes
    rlp::Bytes output_bytes;
    auto read_result = decoder.read(output_bytes);
    
    // If we successfully read something, try to encode it back
    if (read_result) {
        rlp::RlpEncoder encoder;
        encoder.add(rlp::ByteView(output_bytes));
        auto encoded_result = encoder.GetBytes();
        
        // If encoding succeeded, try to decode again for roundtrip verification
        if (encoded_result) {
            rlp::RlpDecoder decoder2(**encoded_result);
            rlp::Bytes roundtrip_bytes;
            decoder2.read(roundtrip_bytes);
            
            // Verify roundtrip consistency
            if (roundtrip_bytes != output_bytes) {
                // This should never happen - indicates a bug
                __builtin_trap();
            }
        }
    }
    
    // Test 2: Try to decode as integers of various sizes
    rlp::RlpDecoder decoder_int(input);
    uint8_t u8_val;
    decoder_int.read(u8_val);
    
    rlp::RlpDecoder decoder_int16(input);
    uint16_t u16_val;
    decoder_int16.read(u16_val);
    
    rlp::RlpDecoder decoder_int32(input);
    uint32_t u32_val;
    decoder_int32.read(u32_val);
    
    rlp::RlpDecoder decoder_int64(input);
    uint64_t u64_val;
    decoder_int64.read(u64_val);
    
    rlp::RlpDecoder decoder_uint256(input);
    intx::uint256 u256_val;
    decoder_uint256.read(u256_val);
    
    // Test 3: Try to decode as a list
    rlp::RlpDecoder decoder_list(input);
    auto list_header_result = decoder_list.ReadListHeaderBytes();
    
    return 0;
}
