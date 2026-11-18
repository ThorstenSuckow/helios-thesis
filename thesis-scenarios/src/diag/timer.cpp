
#include <chrono>
#include <cstdint>
#include <cstdio>

#if defined(_WIN32)
    #include <Windows.h>
#elif defined(__APPLE__)
    #include <mach/mach_time.h>
#endif

/**
 * @brief Measures the cost of calling steady_clock::now() and the tick-interval on windows and mac platforms..
 *
 *
 */

int main() {

    using Clock = std::chrono::steady_clock;
    constexpr int N = 1'000'000;

    std::printf(
        "steady_clock is_steady=%d period=%lld/%lld\n",
        static_cast<int>(Clock::is_steady),
        static_cast<long long>(Clock::period::num),
        static_cast<long long>(Clock::period::den)
    );

#if defined(_WIN32)

    LARGE_INTEGER frequency{};

    if (QueryPerformanceFrequency(&frequency)) {
        std::printf(
            "QPC frequency=%lld Hz -> %.4f ns/tick\n",
            static_cast<long long>(frequency.QuadPart),
            1'000'000'000.0 / frequency.QuadPart
        );
    }

#elif defined(__APPLE__)

    mach_timebase_info_data_t timeBaseInfo{};
    mach_timebase_info(&timeBaseInfo);

    std::printf(
        "mach timebase numer=%u denom=%u -> %.4f ns/tick\n",
        timeBaseInfo.numer,
        timeBaseInfo.denom,
        static_cast<double>(timeBaseInfo.numer) / timeBaseInfo.denom
    );

#endif

    // Approximate cost of calling Clock::now().
    const auto t0 = Clock::now();

    // volatile prevents the compiler from optimizing the loop away
    volatile std::uint64_t sink = 0;

    for (int i = 0; i < N; ++i) {
        sink = sink + static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
    }
    const auto t1 = Clock::now();
    std::printf(
        "Kosten now(): %.2f ns/Aufruf\n",
        std::chrono::duration<double, std::nano>(t1 - t0).count() / N
    );
}
