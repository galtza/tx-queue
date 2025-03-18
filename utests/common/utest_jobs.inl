/*
    ======
    "base"
    ======
*/

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::utest_job(QUEUE_TYPE& _queue) : queue_(_queue), core_(-1), transaction_attempts_(0) {
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
auto qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::start() -> bool {
    thread_ = thread([this] { run(); });
    if (core_ != -1) {
        set_thread_affinity(thread_, (int)core_);
    }
    return true;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::wait_to_complete() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::set_core(int64_t _core) {
    core_ = _core;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::set_start_time(high_resolution_clock::time_point _start_time) {
    start_time_ = _start_time;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
auto qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::get_duration_ns() -> int64_t {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(total_time_).count();
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
auto qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::get_total_data() -> uint64_t {
    return total_data_;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
auto qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::get_transaction_attempts() -> uint64_t {
    return transaction_attempts_;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
auto qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>::get_hash_str() -> string {
    if constexpr (VERIFICATION == CHECKSUM) {
        qcstudio::checksum::to_string(qcstudio::checksum::to_digest(checksum_hash_status_));
    } else if constexpr (VERIFICATION == SHA256) {
        return qcstudio::sha256::to_string(qcstudio::sha256::to_digest(sha256_hash_status_));
    }
    return "";
}

/*
    =================
    "transmit buffer"
    =================
    A buffer is sent over in random sized chunks. optionally a buffer hash is performed.
*/

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
qcstudio::utest_job_transmit_buffer<QUEUE_TYPE, VERIFICATION>::utest_job_transmit_buffer(QUEUE_TYPE& _queue) : qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>(_queue) {
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_transmit_buffer<QUEUE_TYPE, VERIFICATION>::set_data(const uint8_t* _src_data, uint64_t _src_data_size) {
    src_data_      = _src_data;
    src_data_size_ = _src_data_size;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_transmit_buffer<QUEUE_TYPE, VERIFICATION>::set_minmax_chunk_size(int _min_chunk_size, int _max_chunk_size) {
    min_chunk_size_ = _min_chunk_size;
    max_chunk_size_ = _max_chunk_size;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_transmit_buffer<QUEUE_TYPE, VERIFICATION>::run() {
    using namespace std;
    using namespace chrono;

    // init of random generator

    auto rd             = random_device{};
    auto gen            = mt19937(rd());
    auto dis_chunk_size = uniform_int_distribution<>(min_chunk_size_, max_chunk_size_);
    auto dis_byte       = uniform_int_distribution<>(0, 255);

    // synchronize the starting of the actual job

    auto core_message = stringstream{};
    core_message << "[producer] started transmission on core " << get_current_thread_core() << "!\n ";
    cout << "[producer] waiting to start!\n";
    this_thread::sleep_until(this->start_time_);
    cout << core_message.str();

    // send

    this->total_data_ = 0u;
    auto start_time   = high_resolution_clock::now();
    while (this->total_data_ < src_data_size_) {
        if (auto write_op = tx_write_t(this->queue_)) {
            auto src_addr   = src_data_ + this->total_data_;
            auto chunk_size = min((uint64_t)dis_chunk_size(gen), src_data_size_ - this->total_data_);
            write_op.write(chunk_size);
            write_op.write(src_addr, chunk_size);
            if (!write_op) {
                this->transaction_attempts_++;
                continue;
            }

            if constexpr (VERIFICATION == CHECKSUM) {
                update(this->checksum_hash_status_, src_addr, chunk_size);
            } else if constexpr (VERIFICATION == SHA256) {
                update(this->sha256_hash_status_, src_addr, chunk_size);
            }

            this->total_data_ += chunk_size;
        }
    }
    this->total_time_ = high_resolution_clock::now() - start_time;

    // signal end of buffer

    do {
        if (auto write_op = tx_write_t(this->queue_)) {
            if (write_op.write(uint64_t{0})) {
                break;
            }
        }
    } while (true);

    cout << "[producer] quitting...\n";
}

/*
    ================
    "receive buffer"
    ================
    A buffer is received in chunks. Optionally a buffer hash is performed
*/

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
qcstudio::utest_job_receive_buffer<QUEUE_TYPE, VERIFICATION>::utest_job_receive_buffer(QUEUE_TYPE& _queue) : qcstudio::utest_job<QUEUE_TYPE, VERIFICATION>(_queue) {
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_receive_buffer<QUEUE_TYPE, VERIFICATION>::set_max_chunk_size(uint64_t _max_chunk_size) {
    max_chunk_size_ = _max_chunk_size;
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_receive_buffer<QUEUE_TYPE, VERIFICATION>::run() {
    using namespace std;

    // synchronize the starting of the actual job

    auto core_message = stringstream{};
    core_message << "[consumer] started transmission on core " << get_current_thread_core() << "!\n ";
    cout << "[consumer] waiting to start!\n";
    this_thread::sleep_until(this->start_time_);
    cout << core_message.str();

    // recv

    auto buffer       = make_unique<uint8_t[]>(max_chunk_size_);
    auto start_time   = high_resolution_clock::now();
    this->total_data_ = 0u;
    while (true) {
        if (auto read_op = tx_read_t(this->queue_)) {
            auto chunk_size = uint64_t{};
            read_op.read(chunk_size);
            read_op.read(buffer.get(), chunk_size);
            if (!read_op) {
                this->transaction_attempts_++;
                continue;
            }

            if (chunk_size == 0) {
                break;
            }

            if constexpr (VERIFICATION == CHECKSUM) {
                update(this->checksum_hash_status_, buffer.get(), chunk_size);
            } else if constexpr (VERIFICATION == SHA256) {
                update(this->sha256_hash_status_, buffer.get(), chunk_size);
            }

            this->total_data_ += chunk_size;
        }
    }

    this->total_time_ = high_resolution_clock::now() - start_time;
    cout << "[consumer] quitting!\n";
}

/*
    =========================
    "interactive transmitter"
    =========================
    Press `S` to send a random uint16 number and a uint64_t timestamp
*/

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
qcstudio::utest_job_interactive_transmitter<QUEUE_TYPE, VERIFICATION>::utest_job_interactive_transmitter(QUEUE_TYPE& _queue) : qcstudio::utest_job<QUEUE_TYPE>(_queue) {
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_interactive_transmitter<QUEUE_TYPE, VERIFICATION>::run() {
    using namespace std;
    using namespace chrono;

    cout << get_timestamp_str(system_clock::now()) << " [producer] Press/Release key `S` to send a random number and the timestamp" << endl;
    while (true) {
        wait_until_key_release('S');
        auto number    = get_random<uint16_t>();
        auto now       = high_resolution_clock::now();
        auto ok        = false;
        auto timestamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
        if (auto tx = tx_write_t(this->queue_)) {
            tx.write(number, timestamp);
            ok = true;
        } else {
            cout << "[producer] Error sending!" << endl;
        }

        if (ok) {
            cout << get_timestamp_str(system_clock::now()) << "|"                                    //
                 << get_current_thread_core() << " [producer] Just sent number 0x" << hex << number  //
                 << " with timestamp " << dec << timestamp << "!"                                    //
                 << endl;                                                                            //
        }
    }
}

/*
    ======================
    "interactive receiver"
    ======================
*/

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
qcstudio::utest_job_interactive_receiver<QUEUE_TYPE, VERIFICATION>::utest_job_interactive_receiver(QUEUE_TYPE& _queue) : qcstudio::utest_job<QUEUE_TYPE>(_queue) {
}

template<typename QUEUE_TYPE, qcstudio::everification VERIFICATION>
void qcstudio::utest_job_interactive_receiver<QUEUE_TYPE, VERIFICATION>::run() {
    using namespace std;
    using namespace chrono;
    while (true) {
        if (auto tx = tx_read_t(this->queue_)) {
            if (auto [number, timestamp] = tx.read<uint16_t, int64_t>(); tx) {
                auto now_ns = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
                cout << get_timestamp_str(system_clock::now()) << "|"                  //
                     << get_current_thread_core() << " [consumer] Just received \"0x"  //
                     << hex << number << "\", " << dec << timestamp << " (diff = "     //
                     << (now_ns - timestamp) << " ns)"                                 //
                     << endl;                                                          //
            }
        }
    }
}
