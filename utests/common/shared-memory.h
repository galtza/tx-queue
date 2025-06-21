// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

#pragma once

// C++

#include <cstdint>
#include <utility>
#include <string>

// Windows

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace qcstudio {

    /*
        Shared memory

        Features:
        ● Specify buffer size to be the producer, others are consumers
        ● Naturally cache-line-aligned
        ● Contains cache-line header with buffer size
    */

    class shared_memory {
    public:
        shared_memory() = delete;
        shared_memory(const wchar_t* _name, uint64_t _size = 0);  // If no size is specified, it is an `open` operation
        ~shared_memory();

        void* operator*();
        auto  get_size() const -> uint64_t;

    private:
        void create_buffer();
        void open_buffer();
        void lock();
        void unlock();

        friend class shared_memory_lock_guard;

        const wchar_t* name_       = nullptr;
        char*          map_buffer_ = nullptr;
        HANDLE         map_file_   = INVALID_HANDLE_VALUE;
        uint64_t       size_       = 0;
        bool           create_     = true;
    };

}  // namespace qcstudio

#include "shared-memory.inl"
