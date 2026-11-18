module;

#include <cstddef>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

export module thesis.config;

import helios.math;


export namespace thesis::config {

    constexpr auto LOGGING_ENABLED       = false;
    constexpr auto TARGET_FPS            = 0.0F;
    constexpr auto FIXED_DELTA_TIME      = 1.0F / 60.0F;
    constexpr auto COOLDOWN_FRAMES       = 0;
    constexpr auto BORDER_SCALE          = helios::math::vec3f{1.0F, 1.0F, 1.0F};
    constexpr auto TILE_SIZE             = 2;
    constexpr auto PARTICLE_SPAWN_COUNT  = 2;
    constexpr auto PARTICLE_SPEED        = 32.0F;
    constexpr auto GAME_OBJECT_SPEED     = 1.0f;
    constexpr auto PARTICLE_LIFETIME     = 1.0f;

    /**
     * @brief Run parameters, set once at program start (see init()).
     *
     * Derived values (axis length, cell size, spatial grid) are computed from
     * the object count.
     */
    struct RunConfig {
        int warmupFrames   = 1000;
        int measureFrames  = 10000;
        int objectsPerAxis = 300;

        std::size_t threadCount = std::thread::hardware_concurrency() - 1U;
        std::chrono::milliseconds workerWait{60};
        std::size_t spinDuration{1024};
        std::size_t maxPolling{1};

        int axisLength = 0;
        int cellSize   = 0;
        helios::math::aabbf spatialGrid{};

        [[nodiscard]] std::size_t objectCount() const noexcept {
            return static_cast<std::size_t>(objectsPerAxis) * static_cast<std::size_t>(objectsPerAxis);
        }

        void derive() {
            axisLength  = TILE_SIZE * objectsPerAxis + TILE_SIZE - 1;
            cellSize    = axisLength / objectsPerAxis;
            spatialGrid = helios::math::aabbf{
                -axisLength / 2.0F, -axisLength / 2.0F, 0.0f,
                axisLength / 2.0F, axisLength / 2.0F, 0.0f
            };
        }
    };

    /**
     * @brief Global run configuration; call init() before first use.
     */
    inline RunConfig& run() {
        static RunConfig config = [] { RunConfig c; c.derive(); return c; }();
        return config;
    }

    /**
     * @brief Parses `--n <objects> --warmup <frames> --measure <frames> [--worker-wait <ms>]`.
     *
     * `--n` is the total number of GameObjects and must be a perfect square.
     * `--threads` sets the number of ThreadPool worker threads (default: hardware_concurrency - 1; 0 = no workers).
     * `--worker-wait` is the time in milliseconds an idle ThreadPool worker keeps polling
     * before it goes to sleep (0 = sleep immediately).
     *
     * @return false (message on stderr) on invalid input.
     */
    inline bool init(int argc, char** argv) {
        auto& cfg = run();

        auto toLong = [](const char* text, long& out) {
            char* end = nullptr;
            out = std::strtol(text, &end, 10);
            return end != text && *end == '\0' && out >= 0;
        };

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            long value = 0;
            const bool known = arg == "--n" || arg == "--warmup" || arg == "--measure" || arg == "--worker-wait" || arg == "--spin-duration" || arg == "--max-polling" || arg == "--threads";
            if (!known || i + 1 >= argc || !toLong(argv[i + 1], value)
                || (value == 0 && arg != "--worker-wait" && arg != "--spin-duration" && arg != "--max-polling" && arg != "--threads")) {
                std::cerr << "Usage: " << argv[0]
                          << " [--n <objects>] [--warmup <frames>] [--measure <frames>]"
                             " [--worker-wait <ms>] [--spin-duration <n>] [--max-polling <n>] [--threads <count>]" << std::endl;
                return false;
            }
            ++i;

            if (arg == "--threads") { cfg.threadCount = static_cast<std::size_t>(value); }
            if (arg == "--worker-wait") { cfg.workerWait = std::chrono::milliseconds{value}; }
            if (arg == "--spin-duration") { cfg.spinDuration = static_cast<std::size_t>(value); }
            if (arg == "--max-polling") { cfg.maxPolling = static_cast<std::size_t>(value); }
            if (arg == "--warmup")  { cfg.warmupFrames  = static_cast<int>(value); }
            if (arg == "--measure") { cfg.measureFrames = static_cast<int>(value); }
            if (arg == "--n") {
                const long axis = std::lround(std::sqrt(static_cast<double>(value)));
                if (axis * axis != value) {
                    std::cerr << "--n must be a perfect square: " << value << std::endl;
                    return false;
                }
                cfg.objectsPerAxis = static_cast<int>(axis);
            }
        }

        cfg.derive();
        return true;
    }

        template<typename T>
        std::vector<T> resizedVector(
            const std::size_t capacity = run().warmupFrames + run().measureFrames + COOLDOWN_FRAMES
        ) {
            std::vector<T> vec;
            vec.resize(capacity);
            return vec;
        }


        struct METRICS {
            std::vector<std::chrono::nanoseconds::rep> M_COL_TIMINGS       = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> M_LFT_TIMINGS       = resizedVector<std::chrono::nanoseconds::rep>();

            std::vector<std::size_t> M_COL_THREAD_IDS       = resizedVector<std::size_t>();
            std::vector<std::size_t> M_LFT_THREAD_IDS       = resizedVector<std::size_t>();

            std::vector<std::chrono::nanoseconds::rep> M_COL_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> M_LFT_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();

            std::vector<std::chrono::nanoseconds::rep> S_BETA_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::size_t> S_BETA_THREAD_IDS = resizedVector<std::size_t>();
            std::vector<std::chrono::nanoseconds::rep> S_BETA_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();

            std::vector<std::chrono::nanoseconds::rep> S_DELTA_DET_GO_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_DET_PRT_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_RES_GO_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_RES_PRT_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_DET_GO_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_DET_PRT_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_RES_GO_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> S_DELTA_RES_PRT_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();

            std::vector<std::size_t> S_DELTA_DET_GO_THREAD_IDS = resizedVector<std::size_t>();
            std::vector<std::size_t> S_DELTA_DET_PRT_THREAD_IDS = resizedVector<std::size_t>();
            std::vector<std::size_t> S_DELTA_RES_GO_THREAD_IDS = resizedVector<std::size_t>();
            std::vector<std::size_t> S_DELTA_RES_PRT_THREAD_IDS = resizedVector<std::size_t>();

            std::vector<std::chrono::nanoseconds::rep> S_LAMBDA_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::size_t> S_LAMBDA_THREAD_IDS = resizedVector<std::size_t>();
            std::vector<std::chrono::nanoseconds::rep> S_LAMBDA_FLUSH_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();

            // detection / response phase (systems incl. flush; v5: measured parallel group, v1-v3: systems + flush summed)
            std::vector<std::chrono::nanoseconds::rep> P_DET_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();
            std::vector<std::chrono::nanoseconds::rep> P_RES_TIMINGS = resizedVector<std::chrono::nanoseconds::rep>();

            std::vector<std::chrono::nanoseconds::rep> SCHEDULE_TIMINGS    = resizedVector<std::chrono::nanoseconds::rep>();

            std::vector<std::size_t> COLLISION_PAIRS_NARROW_FOUND_GO      = resizedVector<std::size_t>();
            std::vector<std::size_t> COLLISION_PAIRS_NARROW_FOUND_PRT      = resizedVector<std::size_t>();

            std::vector<std::size_t> COLLISION_PAIRS_FOUND_GO      = resizedVector<std::size_t>();
            std::vector<std::size_t> COLLISION_PAIRS_FOUND_PRT     = resizedVector<std::size_t>();
            std::vector<std::size_t> COLLISION_PAIRS_PROCESSED_GO  = resizedVector<std::size_t>();
            std::vector<std::size_t> COLLISION_PAIRS_PROCESSED_PRT = resizedVector<std::size_t>();
            std::vector<std::size_t> COLLISION_COMMANDS_GO         = resizedVector<std::size_t>();
            std::vector<std::size_t> LIFETIME_COMMANDS_PRT         = resizedVector<std::size_t>();
            std::vector<std::size_t> PARTICLE_POPULATION           = resizedVector<std::size_t>();

            std::vector<helios::math::vec3f> POSITION_GO = resizedVector<helios::math::vec3f>(0);
            std::vector<helios::math::vec3f> POSITION_PRT = resizedVector<helios::math::vec3f>(0);
            std::vector<helios::math::vec3f> VELOCITY_GO = resizedVector<helios::math::vec3f>(0);
            std::vector<helios::math::vec3f> VELOCITY_PRT = resizedVector<helios::math::vec3f>(0);


        };;
}