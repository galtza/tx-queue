// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

// QCStudio

#include "shared-memory.h"
#include "tx-queue.h"
#include "utest_jobs.h"

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

constexpr auto k_sample_size    = (uint64_t)1_GiB;
constexpr auto k_queue_size     = (uint64_t)16_KiB;
constexpr auto k_max_chunk_size = (uint64_t)8_KiB;

// global data

auto main(int _argc, const char* _argv[]) -> int {
    // construct the shared memory object

    auto share_memory_size = sizeof(qcstudio::tx_queue_status_t) + k_queue_size;
    auto shared_memory     = qcstudio::shared_memory(L"7d6c10f2740141fa83246ab214618c6d", share_memory_size);

    // construct the queue directly on the shared memory (It should cover 3 cache lines; 64 * 3 = 192)

    auto queue = tx_queue_mp_t((uint8_t*)*shared_memory, share_memory_size);

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

        const auto start_time   = high_resolution_clock::now() + 3s;
        auto       producer_job = utest_job_transmit_buffer<decltype(_queue), VERIFICATION>(_queue);

        producer_job.set_start_time(start_time);
        producer_job.set_data(sample_data.get(), k_sample_size);
        producer_job.set_minmax_chunk_size(147, k_max_chunk_size);

        // write the start time to the queue itself

        if (auto tx = tx_write_t(_queue)) {
            auto timestamp = duration_cast<nanoseconds>(start_time.time_since_epoch()).count();
            tx.write(timestamp);
        } else {
            cout << "Error sending the timestamp to start\n";
            return -1;
        }

        // start the threads

        cout << "== Running...\n\n";

        producer_job.start();
        producer_job.wait_to_complete();

        // print the results

        if constexpr (VERIFICATION != NONE) {
            cout << "producer hash : " << producer_job.get_hash_str() << endl;
        }

        cout << "\n== Stats...\n\n";
        cout << " producer total throughput: " << format_throughput(producer_job.get_total_data(), producer_job.get_total_duration_ns()) << "\n\n";
        cout << "         producer duration: " << format_duration(producer_job.get_total_duration_ns()) << "\n";
        cout << "          data sample size: " << format_size(producer_job.get_total_data()) << "\n";
        cout << "            queue capacity: " << format_size(_queue.capacity()) << "\n";
        cout << "            max chunk size: " << format_size(k_max_chunk_size) << "\n\n";
        cout << "       # write re-attempts: " << dec << producer_job.get_transaction_attempts() << "\n\n";

        cout << endl;

        return 0;
    }

    auto interactive(tx_queue_mp_t& _queue) -> int {
        auto producer_job = utest_job_interactive_transmitter(_queue);
        producer_job.start();
        producer_job.wait_to_complete();
        return 0;
    }

}  // namespace
