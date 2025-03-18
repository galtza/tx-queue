#pragma once

// C++

#include <atomic>
#include <cstdlib>
#include <new>
#include <tuple>
#include <string>
#include <type_traits>

// preprocessor

#pragma warning(push)
#pragma warning(disable : 4324 4625 5026 4626 5027)

#define QCS_DECLARE_QUEUE_FRIENDS \
    template<typename QTYPE>      \
    friend class tx_write_t;      \
    template<typename QTYPE>      \
    friend class tx_read_t;

namespace qcstudio {

    using namespace std;

    constexpr auto CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

    /*
        `tx-queue-XX` are a high-performance, transaction-based, SPSC and SP/MP queues.

        there are two types:
        ● `tx-queue-sp` for single process queues
        ● `tx-queue-mp` for queues shared between two processed

        notes:
        * capacity must be power of 2
        * memory provided must be initialized (zeroed) and cache-line aligned
    */

    class base_tx_queue_t {
    public:
        auto     is_ok() const -> bool;
        auto     capacity() const -> uint64_t;
        explicit operator bool() const noexcept;

    protected:
        alignas(CACHE_LINE_SIZE) uint8_t* storage_ = nullptr;
        uint64_t capacity_                         = 0;
        QCS_DECLARE_QUEUE_FRIENDS
    };

    struct tx_queue_status_t {
        alignas(CACHE_LINE_SIZE) uint64_t head_;
        alignas(CACHE_LINE_SIZE) uint64_t tail_;
    };

    /*
        single-process transaction queue
    */

    class tx_queue_sp_t : public base_tx_queue_t {
    public:
        tx_queue_sp_t(uint64_t _capacity);
        ~tx_queue_sp_t();

    private:
        tx_queue_status_t status_;
        QCS_DECLARE_QUEUE_FRIENDS
    };

    /*
        multi-process transaction queue
    */

    class tx_queue_mp_t : public base_tx_queue_t {
    public:
        tx_queue_mp_t(uint8_t* _prealloc_and_init, uint64_t _capacity);

    private:

        QCS_DECLARE_QUEUE_FRIENDS
        tx_queue_status_t& status_;
    };

    /*
        write transaction

        notes:
        ● size = 1 cache line
        ● auto-invalidates if a write fails
        ● keep queue constants locally here (storage and capacity)
    */

    template<typename QTYPE>
    class alignas(CACHE_LINE_SIZE) tx_write_t {
    public:
        tx_write_t(QTYPE& _queue);
        ~tx_write_t();
        explicit operator bool() const noexcept;

        // various versions of `write`
        // clang-format off
                                                   auto write(const void* _buffer, uint64_t _size) -> bool;                                   // a raw buffer
        template<typename T>                       auto write(const T& _item) -> bool;                                                        // a normal type
        template<typename T, uint64_t N>           auto write(const T (&_array)[N]) -> bool;                                                  // an array (no trailing '\0' for character arrays)
        template<typename FIRST, typename... REST> auto write(const FIRST& _first, REST... _rest) -> enable_if_t<!is_pointer_v<FIRST>, bool>; // variadic
        // clang-format on

        // invalidate and won't auto-commit

        void invalidate();

    private:
        QTYPE&   queue_;
        uint8_t* storage_;
        uint64_t tail_, cached_head_, capacity_;
        bool     invalidated_ : 1;

        auto imp_write(const void* _buffer, uint64_t _size) -> bool;
    };

    /*
        read transaction

        notes:
        ● size = 1 cache line
        ● auto-invalidates if a read fails
        ● keep queue constants locally here (storage and capacity)
    */

    template<typename QTYPE>
    class alignas(CACHE_LINE_SIZE) tx_read_t {
    public:
        tx_read_t(QTYPE& _queue);
        ~tx_read_t();
        explicit operator bool() const noexcept;

        // various versions of `read`
        // clang-format off
                                  auto read(void* _buffer, uint64_t _size) -> bool;                                             // a buffer
        template<typename T>      auto read(T& _item) -> bool;                                                                  // an individual type
        template<typename...ARGS> auto read() -> enable_if_t<conjunction_v<is_default_constructible<ARGS>...>, tuple<ARGS...>>; // structured-bindings compatible

        // clang-format on

        // invalidate and won't auto-commit

        void invalidate();

    private:
        QTYPE&   queue_;
        uint8_t* storage_;
        uint64_t head_, cached_tail_, capacity_;
        bool     invalidated_ : 1;

        auto imp_read(void* _buffer, uint64_t _size) -> bool;
    };

}  // namespace qcstudio

#include "tx-queue.inl"

#pragma warning(pop)
