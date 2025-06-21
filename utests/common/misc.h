// Copyright © 2017-2025 Raúl Ramos García. All rights reserved.

#pragma once

// C++

#include <cstdint>
#include <chrono>
#include <string>
#include <random>
#include <thread>

// Windows

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// Help literal operators

inline constexpr auto operator""_KiB(uint64_t _amount) -> uint64_t { return 1024u * _amount; }
inline constexpr auto operator""_MiB(uint64_t _amount) -> uint64_t { return 1024u * 1024u * _amount; }
inline constexpr auto operator""_GiB(uint64_t _amount) -> uint64_t { return 1024u * 1024u * 1024u * _amount; }

template<typename Clock, typename Duration = typename Clock::duration>
auto get_timestamp_str(std::chrono::time_point<Clock, Duration> _now) -> std::string {
    using namespace std::chrono;

    // Convert to time_t for local time information
    auto    time_now = Clock::to_time_t(_now);
    std::tm timeinfo;
    localtime_s(&timeinfo, &time_now);

    // Calculate the precise time components
    auto duration_since_epoch = _now.time_since_epoch();
    auto hours_since_midnight = duration_cast<hours>(duration_since_epoch) - hours(timeinfo.tm_hour);
    auto min                  = duration_cast<minutes>(duration_since_epoch) - duration_cast<minutes>(hours_since_midnight);
    auto sec                  = duration_cast<seconds>(duration_since_epoch) - duration_cast<seconds>(min) - duration_cast<seconds>(hours_since_midnight);
    auto ms                   = duration_cast<milliseconds>(duration_since_epoch) - duration_cast<milliseconds>(sec);
    auto us                   = duration_cast<microseconds>(duration_since_epoch) - duration_cast<microseconds>(ms);
    auto ns                   = duration_cast<nanoseconds>(duration_since_epoch) - duration_cast<nanoseconds>(us);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << timeinfo.tm_hour << ":"  // Hour from tm structure for accuracy
        << std::setw(2) << min.count() % 60 << ":"  // Minutes since midnight
        << std::setw(2) << sec.count() % 60 << "."  // Seconds since minute start
        << std::setw(3) << ms.count() % 1000 << "ms "
        << std::setw(3) << (us.count() % 1000) << "us "
        << std::setw(3) << (ns.count() % 1000) << "ns";

    return oss.str();
}

void wait_until_key_release(int _key) {
    using namespace std;
    while (!(GetAsyncKeyState(_key) & 0x8000)) {
        this_thread::sleep_for(10ms);
    }

    while (GetAsyncKeyState(_key) & 0x8000) {
        this_thread::sleep_for(10ms);
    }
}

template<typename T>
auto get_random(T _min = 0x1000, T _max = 0x2000) -> T {
    using namespace std;
    static auto rd   = random_device{};
    static auto rng  = mt19937_64(rd());
    static auto dist = uniform_int_distribution<T>(_min, _max);
    return dist(rng);
}

auto set_thread_affinity(std::thread& _thread, int _core) -> bool {
    if (_core < 0) {
        return false;
    }

    auto thread_handle = _thread.native_handle();
    if (!thread_handle) {
        return false;
    }

    auto mask = (DWORD_PTR)(1ULL << _core);
    if (!SetThreadAffinityMask(thread_handle, mask)) {
        return false;
    }
    return true;
}

inline __declspec(noinline) auto get_current_thread_core() -> int {
    return (int)GetCurrentProcessorNumber();
}

auto format_throughput(uint64_t _bytes, int64_t _ns) -> std::string {
    using namespace std;
    using namespace chrono;

    if (_ns == 0) {
        return "Infinite speed!";
    }

    auto seconds = (double)_ns / 1'000'000'000.0;

    auto unit       = string{};
    auto throughput = (double)_bytes / seconds;
    if (throughput < 1024) {
        unit = "B/s";
    } else if (throughput < 1024ULL * 1024) {
        throughput /= 1024;
        unit = "KiB/s";
    } else if (throughput < 1024ULL * 1024 * 1024) {
        throughput /= 1024 * 1024;
        unit = "MiB/s";
    } else if (throughput < 1024ULL * 1024 * 1024 * 1024) {
        throughput /= 1024 * 1024 * 1024;
        unit = "GiB/s";
    } else {
        throughput /= 1024ULL * 1024 * 1024 * 1024;
        unit = "TiB/s";
    }

    stringstream ss;
    ss << fixed << setprecision(2) << throughput << " " << unit;
    return ss.str();
}

auto format_duration(int64_t _ns) -> std::string {
    using namespace std;
    using namespace chrono;

    auto ns_duration = nanoseconds(_ns);
    auto d           = duration_cast<days>(ns_duration);
    auto h           = duration_cast<hours>(ns_duration % days(1));
    auto min         = duration_cast<minutes>(ns_duration % hours(1));
    auto sec         = duration_cast<seconds>(ns_duration % minutes(1));
    auto ms          = duration_cast<milliseconds>(ns_duration % seconds(1));
    auto us          = duration_cast<microseconds>(ns_duration % milliseconds(1));
    auto ns          = duration_cast<nanoseconds>(ns_duration % microseconds(1));

    ostringstream result;

    bool added = false;

    if (auto val = d.count(); val > 0) {
        result << d.count() << " day";
        if (val > 1) {
            result << "s";
        }
        added = true;
    }

    if (auto val = h.count(); val > 0) {
        result << (added ? ", " : "") << val << " hour";
        if (val > 1) {
            result << "s";
        }
        added = true;
    }

    if (auto val = min.count(); val > 0) {
        result << (added ? ", " : "") << val << " minute";
        if (val > 1) {
            result << "s";
        }
        added = true;
    }

    if (auto val = sec.count(); val > 0) {
        result << (added ? ", " : "") << val << " second";
        if (val > 1) {
            result << "s";
        }
        added = true;
    }

    if (ms.count() > 0) {
        result << (added ? ", " : "") << ms.count() << " ms";
        added = true;
    }

    if (us.count() > 0) {
        result << (added ? ", " : "") << us.count() << " us";
        added = true;
    }

    if (ns.count() > 0) {
        result << (added ? ", " : "") << ns.count() << " ns";
    }

    if (!added) {
        result << "0 nanoseconds";
    }

    return result.str();
}

auto format_size(uint64_t bytes) -> std::string {
    using namespace std;

    const char* sizes[]          = {"Bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
    auto        num_bytes_double = (double)bytes;
    auto        i                = 0u;
    for (; i < 8 && num_bytes_double >= 1024.0; ++i) {
        num_bytes_double /= 1024.0;
    }

    ostringstream result;
    result.precision(2);
    result << fixed << num_bytes_double << " " << sizes[i];
    return result.str();
}
