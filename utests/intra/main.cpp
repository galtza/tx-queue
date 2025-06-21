// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

// us

#include "misc.h"
#include "tx-queue.h"
#include "utest_jobs.h"

// C++

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>

// Windows

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

using namespace std;
using namespace chrono;
using namespace chrono_literals;
using namespace qcstudio;

// constants

constexpr auto k_sample_size    = (uint64_t)1_GiB;
constexpr auto k_queue_size     = (uint64_t)16_KiB;
constexpr auto k_max_chunk_size = (uint64_t)8_KiB;

// local tests

namespace {

    template<qcstudio::everification VERIFICATION = NONE>
    auto transmision(tx_queue_sp_t& _queue) -> int;
    auto interactive(tx_queue_sp_t& _queue) -> int;

}

// main procedure

auto main(int _argc, const char* _argv[]) -> int {
    // create a queue for both the producer and the consumer

    auto queue = tx_queue_sp_t(k_queue_size);
    if (!queue) {
        return 1;
    }

    if (_argc >= 2) {
        if (strcmp(_argv[1], "-i") == 0) {
            return interactive(queue);
        } else if (strcmp(_argv[1], "-t") == 0) {
            auto verification = 0u;
            if (_argc > 2) {
                auto sv = string_view(_argv[2]);
                if (sv.starts_with("-v:")) {
                    verification = (unsigned)atoi(sv.substr(3).data());
                }
            }

            switch (verification) {
                case 1: {
                    return transmision<everification::CHECKSUM>(queue);
                }
                case 2: {
                    return transmision<everification::SHA256>(queue);
                }
            }
            return transmision(queue);
        }
    }

    return -1;
}

namespace {

    template<qcstudio::everification VERIFICATION>
    auto transmision(tx_queue_sp_t& _queue) -> int {
        // generate random data

        auto sample_data = make_unique<uint8_t[]>(k_sample_size);
        auto rd          = random_device{};
        auto gen         = mt19937(rd());
        auto dis_byte    = uniform_int_distribution<>(0, 255);

        cout << "== Generating random data...\n";
        for (auto i = 0u; i < k_sample_size; ++i) {
            sample_data[i] = (uint8_t)dis_byte(gen);
        }

        // prepare tests

        const auto start_time   = high_resolution_clock::now() + 1s;
        auto       producer_job = utest_job_transmit_buffer<decltype(_queue), VERIFICATION>(_queue);
        auto       consumer_job = utest_job_receive_buffer<decltype(_queue), VERIFICATION>(_queue);

        // producer_job.set_core(0);
        // consumer_job.set_core(0);

        producer_job.set_data(sample_data.get(), k_sample_size);
        producer_job.set_minmax_chunk_size(147, k_max_chunk_size);
        producer_job.set_start_time(start_time);
        consumer_job.set_start_time(start_time);
        consumer_job.set_max_chunk_size(k_max_chunk_size);

        // start the threads

        cout << "== Running...\n\n";

        producer_job.start();
        consumer_job.start();

        producer_job.wait_to_complete();
        consumer_job.wait_to_complete();

        // print the results

        if constexpr (VERIFICATION) {
            cout << "producer hash : " << producer_job.get_hash_str() << endl;
            cout << "consumer hash : " << consumer_job.get_hash_str() << endl;
        }

        cout << "\n== Stats...\n\n";
        cout << "          data sample size: " << format_size(k_sample_size) << "\n";
        cout << "                queue size: " << format_size(k_queue_size) << "\n";
        cout << "            max chunk size: " << format_size(k_max_chunk_size) << "\n\n";
        cout << "         producer duration: " << format_duration(producer_job.get_total_duration_ns()) << "\n";
        cout << " producer total throughput: " << format_throughput(k_sample_size, producer_job.get_total_duration_ns()) << "\n";
        cout << "         consumer duration: " << format_duration(consumer_job.get_total_duration_ns()) << "\n";
        cout << " consumer total throughput: " << format_throughput(k_sample_size, consumer_job.get_total_duration_ns()) << "\n";
        cout << "       # write re-attempts: " << dec << producer_job.get_transaction_attempts() << "\n";
        cout << "        # read re-attempts: " << dec << consumer_job.get_transaction_attempts() << "\n";
        cout << endl;

        if constexpr (VERIFICATION) {
            if (producer_job.get_hash_str() != consumer_job.get_hash_str()) {
                cout << "Error!\n";
                return 1;
            }
        }
        return 0;
    }

    auto interactive(tx_queue_sp_t& _queue) -> int {
        auto producer_job = qcstudio::utest_job_interactive_transmitter(_queue);
        auto consumer_job = qcstudio::utest_job_interactive_receiver(_queue);
        producer_job.start();
        consumer_job.start();
        producer_job.wait_to_complete();
        consumer_job.wait_to_complete();
        return 0;
    }
}  // namespace
