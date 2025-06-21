// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

// QCStudio

#include "shared-memory.h"
#include "tx-queue.h"
#include "utest_jobs.h"

// C++

#include <string_view>

// aliases

using namespace qcstudio;
using namespace std;

// local tests

namespace {

    template<qcstudio::everification VERIFICATION = NONE>
    auto transmision(tx_queue_mp_t& _queue) -> int;
    auto interactive(tx_queue_mp_t& _queue) -> int;

}

// constants

constexpr auto k_max_chunk_size = (uint64_t)8_KiB;  // pass by param

// global data

auto main(int _argc, const char* _argv[]) -> int {
    // wait for the shared memory to be available

    auto       shared_memory = qcstudio::shared_memory(L"7d6c10f2740141fa83246ab214618c6d");
    const auto max_wait_time = 10s;
    auto       t0            = steady_clock::now();
    while (!*shared_memory && (steady_clock::now() - t0) < max_wait_time) {
        this_thread::sleep_for(1s);
    }

    if (!*shared_memory) {
        cout << "Error: could not open the shared memory\n";
        return -1;
    }

    // construct the queue directly on the shared memory (It should cover 3 cache lines; 64 * 3 = 192)

    auto queue = tx_queue_mp_t((uint8_t*)*shared_memory, shared_memory.get_size());

    // check

    if (!queue) {
        cout << "Error: cannot initialize the queue. Check the size (must be aligned and be power of 2) and other parameters\n";
        return -1;
    }

    // launch the appropriate test

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
    auto transmision(tx_queue_mp_t& _queue) -> int {
        // read the start time from the queue itself and prepare the test

        cout << "== Waiting for the start time..." << endl;
        auto timestamp  = uint64_t{0};
        auto start_time = time_point<high_resolution_clock, nanoseconds>{};
        do {
            auto tx = tx_read_t(_queue);
            if (tx) {
                if (!tx.read(timestamp)) {
                    this_thread::sleep_for(100ms);
                } else {
                    start_time = time_point<high_resolution_clock, nanoseconds>(nanoseconds(timestamp));
                }
            }
        } while (timestamp == 0);

        auto consumer_job = utest_job_receive_buffer<tx_queue_mp_t, VERIFICATION>(_queue);

        consumer_job.set_start_time(start_time);
        consumer_job.set_max_chunk_size(k_max_chunk_size);

        // start the threads

        cout << "== Running...\n\n";

        consumer_job.start();
        consumer_job.wait_to_complete();

        // print the results

        if constexpr (VERIFICATION != NONE) {
            cout << "consumer hash : " << consumer_job.get_hash_str() << endl;
        }

        cout << "\n== Stats...\n\n";
        cout << " consumer total throughput: " << format_throughput(consumer_job.get_total_data(), consumer_job.get_total_duration_ns()) << "\n\n";
        cout << "         consumer duration: " << format_duration(consumer_job.get_total_duration_ns()) << "\n";
        cout << "          data sample size: " << format_size(consumer_job.get_total_data()) << "\n";
        cout << "            queue capacity: " << format_size(_queue.capacity()) << "\n";
        cout << "            max chunk size: " << format_size(k_max_chunk_size) << "\n\n";
        cout << "        # read re-attempts: " << dec << consumer_job.get_transaction_attempts() << "\n\n";
        cout << endl;

        return 0;
    }

    auto interactive(tx_queue_mp_t& _queue) -> int {
        auto consumer_job = utest_job_interactive_receiver(_queue);
        consumer_job.start();
        consumer_job.wait_to_complete();
        return 0;
    }

}  // namespace
