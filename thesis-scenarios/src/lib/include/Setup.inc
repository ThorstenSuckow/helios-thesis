// Scenarios read schedule, runtime system and parallel system group timings (scheduler.timings(...)).
// Requires helios-ecs-config.h to be included before this file.
#if (HELIOS_PERFORMANCE & HELIOS_PERFORMANCE_ALL) != HELIOS_PERFORMANCE_ALL
#error "This scenario requires HELIOS_PERFORMANCE=7 (SCHEDULES | RUNTIME_SYSTEMS | PARALLEL_SYSTEMGROUPS)."
#endif

if (!thesis::config::init(argc, argv)) {
    return 2;
}


auto METRICS = thesis::config::METRICS{};


auto engineRuntime = thesis::bootstrap::bootstrapGameWorld();
auto& ecsDataContainer = engineRuntime->gameWorld.resourceRegistry();
auto& rrr = ecsDataContainer.get<helios::ecs::common::RuntimeResultRegistry>();

auto scheduler = helios::ecs::scheduling::Scheduler();
ecsDataContainer.emplace<UpdateContext>(0.0F, 0.0F, 0, helios::engine::input::InputSnapshot{});
scheduler.init(ecsDataContainer);

FrameTiming frameTiming{};
#if HEADLESS
// Without FramePacer, frameTiming is never updated; enforce the fixed time step explicitly.
frameTiming.deltaTime = thesis::config::FIXED_DELTA_TIME;
#endif
std::size_t frameCount = 0;
float totalTime = 0.0f;

thesis::util::prepare(
    ecsDataContainer, thesis::config::run().threadCount,
    thesis::config::run().workerWait,
    thesis::config::run().spinDuration,
    thesis::config::run().maxPolling
);