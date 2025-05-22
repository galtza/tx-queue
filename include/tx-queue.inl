#define CPY(_dest, _src, _n) memcpy(_dest, _src, _n)

/*
    ====
    Base
    ====
*/

__forceinline auto qcstudio::base_tx_queue_t::is_ok() const -> bool {
    return storage_ != nullptr;
}

__forceinline auto qcstudio::base_tx_queue_t::capacity() const -> uint64_t {
    return capacity_ - 1;
}

__forceinline qcstudio::base_tx_queue_t::operator bool() const noexcept {
    return is_ok();
}

/*
    ==
    SP
    ==
*/

__forceinline qcstudio::tx_queue_sp_t::tx_queue_sp_t(uint64_t _capacity) {
    // basic checks

    if (_capacity < CACHE_LINE_SIZE) {
        return;
    }

    // init indices

    atomic_ref<uint64_t>(status_.head_).store(0);
    atomic_ref<uint64_t>(status_.tail_).store(0);

    // force capacity power of 2

    capacity_ = _capacity;
    capacity_--;
    capacity_ |= capacity_ >> 1;
    capacity_ |= capacity_ >> 2;
    capacity_ |= capacity_ >> 4;
    capacity_ |= capacity_ >> 8;
    capacity_ |= capacity_ >> 16;
    capacity_ |= capacity_ >> 32;
    capacity_++;

    // alloc

#if _WIN32
    storage_ = (uint8_t*)_aligned_malloc(capacity_, CACHE_LINE_SIZE);
#else
    storage_ = (uint8_t*)aligned_alloc(CACHE_LINE_SIZE, capacity_);
#endif
}

__forceinline qcstudio::tx_queue_sp_t::~tx_queue_sp_t() {
    if (storage_) {
#if _WIN32
        _aligned_free(storage_);
#else
        free(storage_);
#endif
    }
}

/*
    ==
    MP
    ==
*/

__forceinline qcstudio::tx_queue_mp_t::tx_queue_mp_t(uint8_t* _prealloc_and_init, uint64_t _capacity) : status_(*new(_prealloc_and_init) tx_queue_status_t) {
    // basic checks

    if (!_prealloc_and_init) {
        return;
    }

    /*
        notes:
        ● the passed storage must include room for indices
        ● the passed capacity must include the size of the indices
        ● the actual storage must me aligned to the size of the cache line
        ● the actual capacity must be power of 2
    */

    auto actual_storage  = _prealloc_and_init + sizeof(tx_queue_status_t);
    auto actual_capacity = _capacity - sizeof(tx_queue_status_t);
    if (((uintptr_t)actual_storage & (CACHE_LINE_SIZE - 1)) != 0 ||  // check alignment
        (actual_capacity & (actual_capacity - 1)) != 0               // check pow of 2
    ) {
        return;
    }

    // init

    storage_  = actual_storage;
    capacity_ = actual_capacity;
}

/*
    =================
    Write transaction
    =================
*/

template<typename QTYPE>
__forceinline qcstudio::tx_write_t<QTYPE>::tx_write_t(QTYPE& _queue) : queue_(_queue) {
    storage_     = queue_.storage_;
    tail_        = atomic_ref<uint64_t>(queue_.status_.tail_).load(memory_order_relaxed);  // relaxed => no sync required as the tail is only modified by the producer (us)
    cached_head_ = atomic_ref<uint64_t>(queue_.status_.head_).load(memory_order_relaxed);  // optimistic guess, "gimme whatever you have". Later we'll sync if required!
    capacity_    = queue_.capacity_;                                                       // copy to favour the data locality
    invalidated_ = !_queue.is_ok();
}

template<typename QTYPE>
__forceinline qcstudio::tx_write_t<QTYPE>::operator bool() const noexcept {
    return !invalidated_;
}

template<typename QTYPE>
__forceinline auto qcstudio::tx_write_t<QTYPE>::write(const void* _buffer, uint64_t _size) -> bool {
    return imp_write(_buffer, _size);
}

template<typename QTYPE>
template<typename T>
__forceinline auto qcstudio::tx_write_t<QTYPE>::write(const T& _item) -> bool {
    if constexpr (is_same_v<T, string>) {
        return imp_write(_item.data(), _item.length());
    } else {
        return imp_write(&_item, sizeof(T));
    }
}

template<typename QTYPE>
template<typename T, uint64_t N>
__forceinline auto qcstudio::tx_write_t<QTYPE>::write(const T (&_array)[N]) -> bool {
    if constexpr (is_same_v<T, char> || is_same_v<T, wchar_t>) {
        return N > 1 && imp_write(&_array[0], (N - 1) * sizeof(T));  // no "\0" included
    } else {
        return imp_write(&_array[0], N * sizeof(T));
    }
}

template<typename QTYPE>
template<typename FIRST, typename... REST>
__forceinline auto qcstudio::tx_write_t<QTYPE>::write(const FIRST& _first, REST... _rest) -> enable_if_t<!is_pointer_v<FIRST>, bool> {
    if (!write(_first)) {
        return false;
    }
    return write(_rest...);
}

template<typename QTYPE>
__forceinline auto qcstudio::tx_write_t<QTYPE>::imp_write(const void* _buffer, uint64_t _size) -> bool {
    if (invalidated_) {
        return false;
    }

    auto available_space = (cached_head_ - tail_ - 1 + capacity_) & (capacity_ - 1);

    // sync the head if no space

    if (_size > available_space) {
        cached_head_    = atomic_ref<uint64_t>(queue_.status_.head_).load(memory_order_acquire);
        available_space = (cached_head_ - tail_ - 1 + capacity_) & (capacity_ - 1);
        if (_size > available_space) {
            invalidated_ = true;
            return false;
        }
    }

    // there is room, hence, write
    // TODO: optimize memcpy with intrinsics

    if ((tail_ + _size) > capacity_) {
        const auto first_chunk_size = capacity_ - tail_;
        CPY(storage_ + tail_, _buffer, /*                        */ first_chunk_size);
        CPY(storage_, /*   */ (uint8_t*)_buffer + first_chunk_size, _size - first_chunk_size);
    } else {
        CPY(storage_ + tail_, _buffer, _size);
    }

    // update the tail properly

    tail_ = (tail_ + _size) & (capacity_ - 1);
    return true;
}

template<typename QTYPE>
__forceinline void qcstudio::tx_write_t<QTYPE>::invalidate() {
    invalidated_ = true;
}

template<typename QTYPE>
__forceinline qcstudio::tx_write_t<QTYPE>::~tx_write_t() {
    if (!invalidated_) {
        atomic_ref<uint64_t>(queue_.status_.tail_).store(tail_, memory_order_release);  // TODO: check how to deal with this in IPC we need to use https://learn.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
    }
}

/*
    =================
    read transaction
    =================
*/

template<typename QTYPE>
__forceinline qcstudio::tx_read_t<QTYPE>::tx_read_t(QTYPE& _queue) : queue_(_queue) {
    storage_     = queue_.storage_;
    head_        = atomic_ref<uint64_t>(queue_.status_.head_).load(memory_order_relaxed);  // relaxed => no sync required as the head is only modified by the consumer (us)
    cached_tail_ = atomic_ref<uint64_t>(queue_.status_.tail_).load(memory_order_relaxed);  // optimistic guess, "gimme whatever you have". Later we'll sync if required!
    capacity_    = queue_.capacity_;                                                       // copy to favour the data locality
    invalidated_ = !_queue.is_ok();
}

template<typename QTYPE>
__forceinline qcstudio::tx_read_t<QTYPE>::operator bool() const noexcept {
    return !invalidated_;
}

template<typename QTYPE>
__forceinline auto qcstudio::tx_read_t<QTYPE>::read(void* _buffer, uint64_t _size) -> bool {
    return imp_read(_buffer, _size);
}

template<typename QTYPE>
template<typename T>
__forceinline auto qcstudio::tx_read_t<QTYPE>::read(T& _item) -> bool {
    return imp_read(&_item, sizeof(T));
}

// Helper function to convert a pointer to a reference

template<typename QTYPE>
template<typename... ARGS>
__forceinline auto qcstudio::tx_read_t<QTYPE>::read() -> enable_if_t<conjunction_v<is_default_constructible<ARGS>...>, tuple<ARGS...>> {
    if constexpr (sizeof...(ARGS) > 0) {
        auto temp = tuple<ARGS...>{};
        if (auto all_read = apply([this](auto&&... _args) { return (this->read(_args) && ...); }, temp)) {
            return move(temp);
        }
    }
    return {};
}

template<typename QTYPE>
__forceinline auto qcstudio::tx_read_t<QTYPE>::imp_read(void* _buffer, uint64_t _size) -> bool {
    if (invalidated_) {
        return false;
    }

    auto available_data = (cached_tail_ - head_ + capacity_) & (capacity_ - 1);

    // sync the head if no space

    if (_size > available_data) {
        cached_tail_   = atomic_ref<uint64_t>(queue_.status_.tail_).load(memory_order_acquire);
        available_data = (cached_tail_ - head_ + capacity_) & (capacity_ - 1);
        if (_size > available_data) {
            invalidated_ = true;
            return false;
        }
    }

    // there is data, hence, read
    // TODO: optimize memcpy with intrinsics

    if ((head_ + _size) > capacity_) {
        const auto first_chunk_size = capacity_ - head_;
        CPY(_buffer, /*                        */ storage_ + head_, first_chunk_size);
        CPY((uint8_t*)_buffer + first_chunk_size, storage_, /*   */ _size - first_chunk_size);
    } else {
        CPY(_buffer, storage_ + head_, _size);
    }

    // update the tail properly

    head_ = (head_ + _size) & (capacity_ - 1);
    return true;
}

template<typename QTYPE>
__forceinline void qcstudio::tx_read_t<QTYPE>::invalidate() {
    invalidated_ = true;
}

template<typename QTYPE>
__forceinline qcstudio::tx_read_t<QTYPE>::~tx_read_t() {
    if (!invalidated_) {
        atomic_ref<uint64_t>(queue_.status_.head_).store(head_, memory_order_release);  // TODO: check how to deal with this in IPC we need to use https://learn.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
    }
}
