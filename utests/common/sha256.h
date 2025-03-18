#include <cstdlib>
#include <new>
#include <array>

#pragma warning(push)
#pragma warning(disable : 4324)  // remove padding warning

namespace qcstudio::sha256 {

    using namespace std;

    /*
        How to...

        Compute sha256 of a buffer B (const uint8_t*) with size S (uint64_t):

        auto status = qcstudio::sha256::status_t{};
        qcstudio::sha256::update(status, B, S);
        ... // repeat as much as we want
        auto digest = qcstudio::sha256::to_digest(status);
     */

    struct digest_t {
        uint64_t a, b, c, d;
        auto     operator==(const digest_t& _other) const -> bool;
    };

    struct alignas(hardware_destructive_interference_size) status_t {
        uint8_t  curr_block[64];
        uint64_t total_num_bits = 0;
        uint32_t h[8]           = {0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19};
        uint64_t cur            = 0;
    };

    void update(status_t& _status, const uint8_t* _buffer, uint64_t _size);
    auto to_digest(status_t& _status) -> digest_t;
    auto to_string(const digest_t& _digest) -> string;
}

// clang-format off

namespace {
    using namespace std;
    using namespace qcstudio::sha256;

    constexpr array<uint32_t, 64> k = {{
        0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
        0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
        0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
        0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
        0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
        0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
        0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
        0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
    }};

    auto rotr(uint32_t _x, uint32_t _n)             -> uint32_t;
    auto sigma0(uint32_t _x)                        -> uint32_t;
    auto sigma1(uint32_t _x)                        -> uint32_t;
    auto sigma0_256(uint32_t _x)                    -> uint32_t;
    auto sigma1_256(uint32_t _x)                    -> uint32_t;
    auto ch(uint32_t _x, uint32_t _y, uint32_t _z)  -> uint32_t;
    auto maj(uint32_t _x, uint32_t _y, uint32_t _z) -> uint32_t;
    auto portable_bswap64(uint64_t _value)          -> uint64_t;

    void process_block(status_t& _status);
}

// clang-format on

inline void qcstudio::sha256::update(status_t& _status, const uint8_t* _buffer, uint64_t _size) {
    auto pc  = _buffer;
    auto eob = _buffer + _size;
    while (pc < eob) {
        const auto available     = 64ull - _status.cur;
        const auto bytes_to_copy = min(available, (uint64_t)(eob - pc));
        memcpy(&_status.curr_block[_status.cur], pc, (size_t)bytes_to_copy);
        _status.cur += bytes_to_copy;
        _status.total_num_bits += bytes_to_copy * 8;
        pc += bytes_to_copy;
        if (_status.cur >= 64) {
            process_block(_status);
            _status.cur = 0;
        }
    }
}

inline auto qcstudio::sha256::to_digest(status_t& _status) -> qcstudio::sha256::digest_t {
    // add end tag (there is always room for 1 byte)

    _status.curr_block[_status.cur++] = 0x80;

    // add padding (from 0 to potentially up to 2 blocks)

    if (const auto after_end_zeros = ((55 - _status.total_num_bits / 8) % 64 + 64) % 64) {
        if (const auto remaining_in_block = 64 - _status.cur; after_end_zeros > remaining_in_block) {
            memset(_status.curr_block + _status.cur, 0, remaining_in_block);
            process_block(_status);
            memset(_status.curr_block, 0, after_end_zeros - remaining_in_block);
            _status.cur = after_end_zeros - remaining_in_block;
        } else {
            memset(&_status.curr_block[_status.cur], 0, after_end_zeros);
            _status.cur += after_end_zeros;
        }
    }

    // add length

    *reinterpret_cast<uint64_t*>(&_status.curr_block[_status.cur]) = portable_bswap64(_status.total_num_bits);
    process_block(_status);

    // compute/return the digest

    return digest_t{
        (static_cast<uint64_t>(_status.h[0]) << 32) | _status.h[1],  // a
        (static_cast<uint64_t>(_status.h[2]) << 32) | _status.h[3],  // b
        (static_cast<uint64_t>(_status.h[4]) << 32) | _status.h[5],  // c
        (static_cast<uint64_t>(_status.h[6]) << 32) | _status.h[7]   // d
    };
}

auto qcstudio::sha256::to_string(const digest_t& _digest) -> std::string {
    using namespace std;
    ostringstream oss;
    oss << hex << setfill('0')
        << setw(16) << _digest.a
        << setw(16) << _digest.b
        << setw(16) << _digest.c
        << setw(16) << _digest.d;
    return oss.str();
}

inline auto qcstudio::sha256::digest_t::operator==(const digest_t& _other) const -> bool {
    return a == _other.a && b == _other.b && c == _other.c && d == _other.d;
}

namespace {

    using namespace qcstudio::sha256;

    void process_block(qcstudio::sha256::status_t& _status) {
        uint32_t w[64];
        for (auto j = 0u; j < 16; ++j) {
            w[j] = (uint32_t)(_status.curr_block[j * 4 + 0] << 24)  //
                 | (uint32_t)(_status.curr_block[j * 4 + 1] << 16)  //
                 | (uint32_t)(_status.curr_block[j * 4 + 2] << 8)   //
                 | (uint32_t)(_status.curr_block[j * 4 + 3]         //
                 );
        }

        for (auto j = 16u; j < 64u; ++j) {
            w[j] = sigma1_256(w[j - 2]) + w[j - 7] + sigma0_256(w[j - 15]) + w[j - 16];
        }

        auto a = _status.h[0], b = _status.h[1], c = _status.h[2], d = _status.h[3], e = _status.h[4], f = _status.h[5], g = _status.h[6], h = _status.h[7];

        for (auto j = 0u; j < 64; ++j) {
            auto temp1 = h + sigma1(e) + ch(e, f, g) + k[j] + w[j];
            auto temp2 = sigma0(a) + maj(a, b, c);
            h          = g;
            g          = f;
            f          = e;
            e          = d + temp1;
            d          = c;
            c          = b;
            b          = a;
            a          = temp1 + temp2;
        }

        _status.h[0] += a;
        _status.h[1] += b;
        _status.h[2] += c;
        _status.h[3] += d;
        _status.h[4] += e;
        _status.h[5] += f;
        _status.h[6] += g;
        _status.h[7] += h;
    }

    inline auto rotr(uint32_t _x, uint32_t _n) /*            */ -> uint32_t { return (_x >> _n) | (_x << (32 - _n)); }
    inline auto sigma0(uint32_t _x) /*                       */ -> uint32_t { return rotr(_x, 2) ^ rotr(_x, 13) ^ rotr(_x, 22); }
    inline auto sigma1(uint32_t _x) /*                       */ -> uint32_t { return rotr(_x, 6) ^ rotr(_x, 11) ^ rotr(_x, 25); }
    inline auto sigma0_256(uint32_t _x) /*                   */ -> uint32_t { return rotr(_x, 7) ^ rotr(_x, 18) ^ (_x >> 3); }
    inline auto sigma1_256(uint32_t _x) /*                   */ -> uint32_t { return rotr(_x, 17) ^ rotr(_x, 19) ^ (_x >> 10); }
    inline auto ch(uint32_t _x, uint32_t _y, uint32_t _z) /* */ -> uint32_t { return (_x & _y) ^ ((~_x) & _z); }
    inline auto maj(uint32_t _x, uint32_t _y, uint32_t _z) /**/ -> uint32_t { return (_x & _y) ^ (_x & _z) ^ (_y & _z); }

    inline auto portable_bswap64(uint64_t _value) -> uint64_t {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_bswap64(_value);
#elif defined(_MSC_VER)
        return _byteswap_uint64(_value);
#else
        return ((_value & 0xFF00000000000000ULL) >> 56)  //
             | ((_value & 0x00FF000000000000ULL) >> 40)  //
             | ((_value & 0x0000FF0000000000ULL) >> 24)  //
             | ((_value & 0x000000FF00000000ULL) >> 8)   //
             | ((_value & 0x00000000FF000000ULL) << 8)   //
             | ((_value & 0x0000000000FF0000ULL) << 24)  //
             | ((_value & 0x000000000000FF00ULL) << 40)  //
             | ((_value & 0x00000000000000FFULL) << 56);
#endif
    }

}  // namespace

#pragma warning(pop)
