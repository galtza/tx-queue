// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

namespace qcstudio {

    using namespace std;

    inline auto shared_memory::get_size() const -> uint64_t {
        return size_;
    }

    inline shared_memory::shared_memory(const wchar_t* _name, uint64_t _size)
        : name_(_name), map_buffer_(nullptr), map_file_(INVALID_HANDLE_VALUE), size_(_size), create_(_size != 0) {
        if (create_) {
            create_buffer();
        } else {
            open_buffer();
        }
    }

    inline shared_memory::~shared_memory() {
        if (map_buffer_) {
            UnmapViewOfFile(map_buffer_);
        }
        if (map_file_ != INVALID_HANDLE_VALUE) {
            CloseHandle(map_file_);
        }
    }

    inline void* shared_memory::operator*() {
        if (!map_buffer_) {
            if (create_) {
                create_buffer();
            } else {
                open_buffer();
            }
        }
        return reinterpret_cast<void*>(map_buffer_);
    }

    inline void shared_memory::create_buffer() {
        const auto split_size = [](uint64_t _size) -> pair<DWORD, DWORD> {
            auto highOrder = static_cast<DWORD>((_size >> 32) & 0xFFffFFff);
            auto lowOrder  = static_cast<DWORD>(_size & 0xFFffFFff);
            return {highOrder, lowOrder};
        };

        SECURITY_ATTRIBUTES sa;
        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
        sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle       = FALSE;
        auto total_size         = size_ + std::hardware_destructive_interference_size;
        auto [hi, lo]           = split_size(total_size);

        map_file_ = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, hi, lo, name_);
        if (map_file_) {
            map_buffer_ = (char*)MapViewOfFile(map_file_, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
        }

        if (map_buffer_) {
            *((decltype(size_)*)map_buffer_) = size_;
            map_buffer_ += std::hardware_destructive_interference_size;
        }
    }

    inline void shared_memory::open_buffer() {
        map_file_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name_);
        if (!map_file_) {
            return;
        }

        map_buffer_ = (char*)MapViewOfFile(map_file_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (map_buffer_) {
            size_ = *(decltype(size_)*)map_buffer_;
            map_buffer_ += std::hardware_destructive_interference_size;
        }
    }

}  // namespace qcstudio
