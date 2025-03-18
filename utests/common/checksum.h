#include <cstdint>
#include <cstring>

namespace qcstudio::checksum {

    using namespace std;

    /*
        How to...

        Compute checksum of a buffer B (const uint8_t*) with size S (uint64_t):

        auto status = qcstudio::checksum::status_t{};
        qcstudio::checksum::update(status, B, S);
        ... // repeat as much as we want
        auto digest = qcstudio::checksum::to_digest(status);
     */

    struct digest_t {
        uint32_t checksum;
        auto     operator==(const digest_t& _other) const;
    };

    struct status_t {
        uint32_t checksum = 0;
    };

    void update(status_t& _status, const uint8_t* _buffer, uint64_t _size);
    auto to_digest(status_t& _status) -> digest_t;
    auto to_string(const digest_t& _digest) -> string;
}

// inline implementation

using namespace qcstudio::checksum;

void qcstudio::checksum::update(qcstudio::checksum::status_t& _status, const uint8_t* _buffer, uint64_t _size) {
    for (auto i = 0u; i < _size; ++i) {
        _status.checksum += _buffer[i];
    }
}

auto qcstudio::checksum::to_digest(status_t& _status) -> qcstudio::checksum::digest_t {
    return digest_t{_status.checksum};
}

auto qcstudio::checksum::to_string(const digest_t& _digest) -> std::string {
    using namespace std;
    ostringstream oss;
    oss << hex << _digest.checksum;
    return oss.str();
}

inline auto qcstudio::checksum::digest_t::operator==(const digest_t& _other) const {
    return checksum == _other.checksum;
}
