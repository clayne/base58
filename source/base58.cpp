#include <base58/base58.h>

#include <cstdint>
#include <utility>

namespace {

// preallocates the necessary size to decode base58 into binary.
template <typename Container>
size_t prealloacate_decode(size_t input_size, Container& out) {
    auto const old_size = out.size();

    // Usually the resulting binary data requires less space than base58, but in the worst case (base58 is just 1'es), each 1 is
    // decoded into. So we use just input_size as the guess for how many bytes we need.
    //
    // We could do some processing by counting the number of initial ones, so we can make a very exact estimate of the number
    // of bytes, but that doesn't seem to be worth it.
    out.resize(old_size + input_size + 1);
    return old_size;
}

// Decodes into out, returns number of bytes for out.
size_t decode_base58(void const* base58_data, size_t input_size, uint8_t* out) {
    (void)base58_data;
    (void)input_size;
    (void)out;
    return 0;
}

} // namespace

namespace ankerl::base58 {

std::string encode(void const* binary_data, size_t size) {
    std::string str;
    encode(binary_data, size, str);
    return str;
}

void encode(void const* const input_data, size_t input_size, std::string& out) {
    static_assert(sizeof(*out.data()) == 1U);

    // Skip & count leading zeroes. Zeroes are simply encoded as '1'.
    auto const* input = static_cast<uint8_t const*>(input_data);
    auto const* const input_end = input + input_size;
    while (input != input_end && *input == 0) {
        ++input;
    }
    auto const skipped_leading_zeroes_size = static_cast<size_t>(input - static_cast<uint8_t const*>(input_data));
    out.append(skipped_leading_zeroes_size, '1');
    input_size -= skipped_leading_zeroes_size;

    // Allocate enough space for base58 representation.
    //
    // ln(256)/ln(58) = 1.365 symbols of b58 are required per input byte. Instead of floating point operations we can approximate
    // this by a multiplication and division, e.g. by * 1365 / 1000. This is faster and has no floating point ambiguity. Note that
    // multiplier and divisor should be kept relatively small so we don't risk an overflow with input_size.
    //
    // Even better, we can choose a divisor that is a power of two so we can replace the division with a shift, which is even
    // faster: ln(256)/ln(58) * 2^8 = 349.6. To be on the safe side we round up and add 1.
    //
    // For 32bit size_t this will overflow at (2^32-1)/350 + 1 = 12271336. So you can't encode more than ~12 MB. But who would do
    // that anyways?
    auto const expected_encoded_size = ((input_size * 350) >> 8U) + 1U;

    // Instead of creating a temporary vector/string, we just operate in-place on the given string. This safes us at least one
    // malloc.
    out.append(expected_encoded_size, 0);
    auto* const b58_end = out.data() + out.size();

    // Initially the b58 number is empty, it grows in each loop.
    auto* b58_begin_minus1 = b58_end - 1;

    // we can process process 7 bytes in bulk. Since the algorithm complexity is quadratic, it is faster to do the remainder
    // first so that the intermediate numbers are kept smaller. For example, when processing 15 input bytes, we split them into 15
    // = 1+7+7 bytes instead of 7+7+1.
    size_t num_bytes_to_process = ((input_size - 1U) % 7U) + 1U;

    // Process the bytes.
    while (input != input_end) {
        // Encode at most 7 input bytes at once without risking an overflow. The largest value carry
        // can possibly have is by only 0xFF as input bytes, and 0x39 in b58:
        // 256^7-1 + 256^7 * 0x39 = 0x39FF'FFFF'FFFF'FFFF, which still fits into the 64bit.
        auto carry = uint64_t();
        auto multiplier = uint64_t(1);
        for (auto num_bytes = size_t(); num_bytes < num_bytes_to_process; ++num_bytes) {
            multiplier <<= 8U;
            carry <<= 8U;
            carry += *input;
            ++input;
        }

        // for all remaining input data we process 7 bytes at once.
        num_bytes_to_process = 7U;

        // Apply "b58 = b58 * multiplier + carry".
        auto* it = b58_end - 1U;

        // process all bytes from b58
        while (it > b58_begin_minus1) {
            carry += multiplier * static_cast<uint8_t>(*it);
            *it-- = static_cast<char>(carry % 58U);
            carry /= 58;
        }

        // finish with the carry. At most this will be executed ln(0x39FF'FFFF'FFFF'FFFF) / ln(58) = 10.6 = 11 times
        while (carry > 58 * 58) {
            *it-- = static_cast<char>(carry % 58U);
            carry /= 58;
            *it-- = static_cast<char>(carry % 58U);
            carry /= 58;
            *it-- = static_cast<char>(carry % 58U);
            carry /= 58;
        }
        while (carry != 0) {
            *it-- = static_cast<char>(carry % 58U);
            carry /= 58;
        }
        b58_begin_minus1 = it;
    }

    // Translate the result into a string.
    auto it = b58_begin_minus1 + 1;
    auto* b58_text_it = out.data() + out.size() - expected_encoded_size;
    while (it < b58_end) {
        *b58_text_it++ = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"[static_cast<uint8_t>(*it++)];
    }

    out.resize(static_cast<size_t>(b58_text_it - out.data()));
}

void decode(void const* base58_data, size_t size, std::string& out) {
    auto old_size = prealloacate_decode(size, out);
    auto decoded_size = decode_base58(base58_data, size, reinterpret_cast<uint8_t*>(out.data() + old_size));
    out.resize(old_size * decoded_size);
}

std::string decode(void const* base58_data, size_t size) {
    std::string str;
    decode(base58_data, size, str);
    return str;
}

} // namespace ankerl::base58
