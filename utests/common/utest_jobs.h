// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

#pragma once

// C++

#include <cstdint>
#include <atomic>
#include <thread>
#include <iostream>

// qcstudio

#include "tx-queue.h"
#include "misc.h"
#include "sha256.h"
#include "checksum.h"

// warnings

#pragma warning(push)
#pragma warning(disable : 4324 4625 4626 5026 5027 4702)

namespace qcstudio {
    using namespace std;
    using namespace chrono;

    enum everification {
        NONE,
        CHECKSUM,
        SHA256
    };

    /*
        ========
        base job
        ========
    */

    template<typename QUEUE_TYPE, everification VERIFICATION = NONE>
    class utest_job {
    public:
        utest_job(QUEUE_TYPE& _queue);
        virtual ~utest_job() {};

        virtual auto start() -> bool;
        void         wait_to_complete();

        // setters

        void set_core(int64_t _core);
        void set_start_time(high_resolution_clock::time_point _start_time);

        // getters

        auto get_total_data() const -> uint64_t;
        auto get_total_duration_ns() const -> int64_t;
        auto get_transaction_attempts() const -> uint64_t;
        auto get_hash_str() const -> string;

    protected:
        QUEUE_TYPE& queue_;
        int64_t     core_;
        uint64_t    transaction_attempts_;
        uint64_t    total_data_;

        unsigned verification_;

        high_resolution_clock::time_point start_time_;
        high_resolution_clock::duration   total_time_;

        checksum::status_t checksum_hash_status_;
        sha256::status_t   sha256_hash_status_;

        virtual void run() = 0;

    private:
        thread thread_;
    };

    /*
        =================
        transmit a buffer
        =================
    */

    template<typename QUEUE_TYPE, everification VERIFICATION = NONE>
    class utest_job_transmit_buffer : public utest_job<QUEUE_TYPE, VERIFICATION> {
    public:
        utest_job_transmit_buffer(QUEUE_TYPE& _queue);

        // setters

        void set_data(const uint8_t* _src_data, uint64_t _src_data_size);
        void set_minmax_chunk_size(uint64_t _min_chunk_size, uint64_t _max_chunk_size);

    private:
        virtual void run();

        const uint8_t* src_data_;
        uint64_t       src_data_size_;
        uint64_t       min_chunk_size_, max_chunk_size_;
    };

    /*
        ================
        receive a buffer
        ================
    */

    template<typename QUEUE_TYPE, everification VERIFICATION = NONE>
    class utest_job_receive_buffer : public utest_job<QUEUE_TYPE, VERIFICATION> {
    public:
        utest_job_receive_buffer(QUEUE_TYPE& _queue);

        // setters

        void set_max_chunk_size(uint64_t _max_chunk_size);

    private:
        virtual void run();

        uint64_t max_chunk_size_;
    };

    /*
        ========================
        interactive transmission
        ========================
    */

    template<typename QUEUE_TYPE, everification VERIFICATION = NONE>
    class utest_job_interactive_transmitter : public utest_job<QUEUE_TYPE, VERIFICATION> {
    public:
        utest_job_interactive_transmitter(QUEUE_TYPE& _queue);

    private:
        virtual void run();
    };

    /*
        =====================
        interactive reception
        =====================
    */

    template<typename QUEUE_TYPE, everification VERIFICATION = NONE>
    class utest_job_interactive_receiver : public utest_job<QUEUE_TYPE, VERIFICATION> {
    public:
        utest_job_interactive_receiver(QUEUE_TYPE& _queue);

    private:
        virtual void run();
    };

}  // namespace qcstudio

#include "utest_jobs.inl"

#pragma warning(pop)
