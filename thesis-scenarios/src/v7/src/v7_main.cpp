
#include <string>
#include "helios-ecs-config.h"
#include <thread>

import helios.core;
import helios.ecs;
import helios.engine;
import helios.math;
import helios.physics;
import helios.gameplay;
import helios.opengl;
import helios.glfw;
import helios.imgui;

import thesis.handles;
import thesis.strategies;
import thesis.bootstrap;
import thesis.config;
import thesis.util;
import thesis.systems;
import thesis.manager;
import thesis.types;


#include "../../lib/include/Namespaces.h"

using namespace thesis::config;

#ifndef HEADLESS
#define HEADLESS false
#endif

int main(int argc, char** argv) {

    std::string PROGRAM_ID = "v7";


    #include "../../lib/include/Setup.inc"

    #include "../../lib/include/RenderSetup.inc"


    using GameObject = GameObjectHandle;
    using Particle = ParticleHandle;

    using DeltaManager = thesis::manager::DeltaManager<GameObject, Particle>;
    using LambdaManager = thesis::manager::LambdaManager<Particle>;
    using BetaSystem = thesis::systems::BetaSystem<GameObject, Particle>;
    constexpr std::size_t THRESHOLD = 10000;
    constexpr std::size_t CHUNK_COUNT =  4;
    constexpr bool USE_THREADS = true;

    using DeltaDetSystemGo = thesis::systems::DeltaDetSystem<
        GameObject, thesis::strategies::GridCollisionDetection<GameObject, USE_THREADS, THRESHOLD, CHUNK_COUNT>
    >;
    using DeltaDetSystemPrt = thesis::systems::DeltaDetSystem<
        Particle, thesis::strategies::GridCollisionDetection<Particle, USE_THREADS, THRESHOLD, CHUNK_COUNT>
    >;
    using DeltaResSystemGo = thesis::systems::DeltaResSystem<GameObject>;
    using DeltaResSystemPrt = thesis::systems::DeltaResSystem<Particle>;
    using LambdaSystem = thesis::systems::LambdaSystem<Particle>;

    using CollisionDetectionResultGo = helios::physics::collision::types::CollisionDetectionResult<GameObject>;
    using CollisionDetectionResultPrt = helios::physics::collision::types::CollisionDetectionResult<Particle>;

    using DeltaResResultGo = thesis::types::DeltaResResult<GameObject>;
    using DeltaResResultPrt = thesis::types::DeltaResResult<Particle>;

    #if HEADLESS
        auto gameObjectRenderCallback = [&](auto& gameObject) {
        };
        auto borderRenderCallback = [&](auto& border) {
        };
        auto particleRenderCallback = [&](auto& particle) {
        };

    #else
        auto gameObjectRenderCallback = [&](auto& gameObject) {
            gameObject.setActive(true);
            gameObject.template add<DefaultSceneMemberComponent>(MainScene);
            gameObject.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), GameObjectMaterial.handle(), RectMesh.handle()
            );
        };

        auto borderRenderCallback = [&](auto& border) {
            border.setActive(true);
            border.template add<DefaultSceneMemberComponent>(MainScene);
            border.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), BoundaryMaterial.handle(), RectMesh.handle()
            );
        };
        auto particleRenderCallback = [&](auto& particle) {
            particle.setActive(true);
            particle.template add<DefaultSceneMemberComponent>(MainScene);
            particle.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), ParticleMaterial.handle(), RectMesh.handle()
            );
        };
    #endif

    auto collisionCallback = thesis::util::collCb<Particle>(particleRenderCallback);

    auto m_delta             = DeltaManager{collisionCallback};
    auto m_lambda             = LambdaManager();
    auto s_beta            = BetaSystem();
    auto s_delta_det_go    = DeltaDetSystemGo(thesis::util::grid<GameObject, USE_THREADS, THRESHOLD, CHUNK_COUNT>());
    auto s_delta_det_prt   = DeltaDetSystemPrt(thesis::util::grid<Particle, USE_THREADS, THRESHOLD, CHUNK_COUNT>());
    auto s_delta_res_go    = DeltaResSystemGo();
    auto s_delta_res_prt   = DeltaResSystemPrt();
    auto s_lambda          = LambdaSystem();

    #include "../../lib/include/ToolingSetup.inc"

#include "../../lib/include/SchedulerPreSetup.inc"


    thesis::util::createGameObjects<GameObject>(
        ecsDataContainer.get<EntityManager<GameObject>>(),
        gameObjectRenderCallback
    );
    thesis::util::createBoundaries<GameObject>(
        ecsDataContainer.get<EntityManager<GameObject>>(),
        borderRenderCallback
    );
    thesis::util::createBoundaries<Particle>(
        ecsDataContainer.get<EntityManager<Particle>>(),
        borderRenderCallback
    );

    // +----------------------------------------
    // | THESIS SCHEDULE
    // +----------------------------------------
    systemSchedule
        .startWith(m_delta)
            .add(s_beta)
            .add(s_delta_det_go, s_delta_det_prt)
            .add(s_delta_res_go, s_delta_res_prt)
            .add(s_lambda)
        .endWith(m_lambda);
    // +----------------------------------------
    // +----------------------------------------

    auto pDetHandle = helios::ecs::scheduling::types::ParallelSystemGroupEntryHandle{
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 1
    };
    auto pResHandle = helios::ecs::scheduling::types::ParallelSystemGroupEntryHandle{
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 2
    };


     auto mColTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::manager::types::ManagerTypeId>{
        helios::ecs::manager::types::ManagerTypeId::template id<DeltaManager>(),
        helios::ecs::scheduling::types::SchedulePhase::Start,
        systemSchedule.idx(), 0, 0, 0,
    };

    auto mLftTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::manager::types::ManagerTypeId>{
        helios::ecs::manager::types::ManagerTypeId::template id<LambdaManager>(),
        helios::ecs::scheduling::types::SchedulePhase::End,
        systemSchedule.idx(), 0, 0, 0,
    };

    auto sBetaTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<BetaSystem>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 0,
    };

    auto sDeltaDetGoTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaDetSystemGo>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 1, 0, 0,
    };

    auto sDeltaDetPrtTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaDetSystemPrt>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 1, 1, 0,
    };

    auto sDeltaResGoTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaResSystemGo>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 2, 0, 0,
    };

    auto sDeltaResPrtTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaResSystemPrt>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 2, 1, 0,
    };

    auto sLambdaTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<LambdaSystem>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 3, 0, 0,
    };

#if !HEADLESS
    scheduler.beginSchedule(GameWorld::sessionState(EngineState::Running))
     .add(
         Sequential(
             SceneMemberVisibilitySystem<
                 GameObjectHandle, Instanced, NullCullingStrategy<GameObjectHandle>, DefaultRenderHandles
             >(NullCullingStrategy<GameObjectHandle>()),
             DefaultSceneRenderSystem<GameObjectHandle, Instanced>()
         ),
         Sequential(
             SceneMemberVisibilitySystem<
                 ParticleHandle, Instanced, NullCullingStrategy<ParticleHandle>, DefaultRenderHandles
             >(NullCullingStrategy<ParticleHandle>()),
             [&](EntityManager<ParticleHandle>& entityManager,
                DefaultSceneMemberVisibilityRegistry<ParticleHandle, Instanced>& visibilityRegistry) {
                 auto* lifetimeSparseSet = entityManager.sparseSet<LifetimeComponent>();
                 visibilityRegistry.forEachVisibleMember([&lifetimeSparseSet = *lifetimeSparseSet](auto& visibleContext) {
                     if (auto* cmp = lifetimeSparseSet.get(visibleContext.memberHandle.entityId())) {
                         visibleContext.normalizedAge = cmp->normalizedAge();
                     }
                 });
             },
             DefaultSceneRenderSystem<ParticleHandle, Instanced>())

         )

 .endWith<Sequential<DefaultRenderManager>>()
#endif

    #include "../../lib/include/SchedulerPostSetup.inc"


    thesis::util::init(ecsDataContainer);

    while (thesis::util::shouldbeRunning(frameCount)) {

        #if !HEADLESS
            framePacer.beginFrame();
        #endif

        scheduler.update();

        #if !HEADLESS
            frameTiming = framePacer.sync();
        #endif

        auto collisionManagerResult = rrr.item<thesis::types::DeltaManagerResult>();
        METRICS.COLLISION_COMMANDS_GO[frameCount] = collisionManagerResult ? collisionManagerResult->collisionCount : 0;

        auto lifetimeManagerResult = rrr.item<thesis::types::LambdaManagerResult>();
        METRICS.LIFETIME_COMMANDS_PRT[frameCount] = lifetimeManagerResult ? lifetimeManagerResult->expiredCount : 0;

        auto collisionDetectionResult_GO = rrr.item<CollisionDetectionResultGo>();
        METRICS.COLLISION_PAIRS_FOUND_GO[frameCount] = collisionDetectionResult_GO ? collisionDetectionResult_GO->collisionPairs().size() : 0;

        auto collisionDetectionResult_PRT = rrr.item<CollisionDetectionResultPrt>();
        METRICS.COLLISION_PAIRS_FOUND_PRT[frameCount] = collisionDetectionResult_PRT ? collisionDetectionResult_PRT->collisionPairs().size() : 0;

        auto collisionProcessedResult_GO = rrr.item<DeltaResResultGo>();
        METRICS.COLLISION_PAIRS_PROCESSED_GO[frameCount] = collisionProcessedResult_GO ? collisionProcessedResult_GO->processedCollisions : 0;

        auto collisionProcessedResult_PRT = rrr.item<DeltaResResultPrt>();
        METRICS.COLLISION_PAIRS_PROCESSED_PRT[frameCount] = collisionProcessedResult_PRT ? collisionProcessedResult_PRT->processedCollisions : 0;

        METRICS.PARTICLE_POPULATION[frameCount] = ecsDataContainer.get<EntityManager<Particle>>()
        .sparseSet<helios::physics::collision::components::CollisionComponent>()->componentCount();

        METRICS.M_COL_TIMINGS[frameCount] = scheduler.timings(mColTimingHandle).updateDuration;
        METRICS.M_LFT_TIMINGS[frameCount] = scheduler.timings(mLftTimingHandle).updateDuration;
        METRICS.S_BETA_TIMINGS[frameCount] = scheduler.timings(sBetaTimingHandle).updateDuration;
        METRICS.S_DELTA_DET_GO_TIMINGS[frameCount] = scheduler.timings(sDeltaDetGoTimingHandle).updateDuration;
        METRICS.S_DELTA_DET_PRT_TIMINGS[frameCount] = scheduler.timings(sDeltaDetPrtTimingHandle).updateDuration;
        METRICS.S_DELTA_RES_GO_TIMINGS[frameCount] = scheduler.timings(sDeltaResGoTimingHandle).updateDuration;
        METRICS.S_DELTA_RES_PRT_TIMINGS[frameCount] = scheduler.timings(sDeltaResPrtTimingHandle).updateDuration;
        METRICS.S_LAMBDA_TIMINGS[frameCount] = scheduler.timings(sLambdaTimingHandle).updateDuration;

        METRICS.M_COL_THREAD_IDS[frameCount] = scheduler.timings(mColTimingHandle).threadId;
        METRICS.M_LFT_THREAD_IDS[frameCount] = scheduler.timings(mLftTimingHandle).threadId;
        METRICS.S_BETA_THREAD_IDS[frameCount] = scheduler.timings(sBetaTimingHandle).threadId;
        METRICS.S_DELTA_DET_GO_THREAD_IDS[frameCount] = scheduler.timings(sDeltaDetGoTimingHandle).threadId;
        METRICS.S_DELTA_DET_PRT_THREAD_IDS[frameCount] = scheduler.timings(sDeltaDetPrtTimingHandle).threadId;
        METRICS.S_DELTA_RES_GO_THREAD_IDS[frameCount] = scheduler.timings(sDeltaResGoTimingHandle).threadId;
        METRICS.S_DELTA_RES_PRT_THREAD_IDS[frameCount] = scheduler.timings(sDeltaResPrtTimingHandle).threadId;
        METRICS.S_LAMBDA_THREAD_IDS[frameCount] = scheduler.timings(sLambdaTimingHandle).threadId;


        METRICS.M_COL_FLUSH_TIMINGS[frameCount] = scheduler.timings(mColTimingHandle).flushDuration;
        METRICS.M_LFT_FLUSH_TIMINGS[frameCount] = scheduler.timings(mLftTimingHandle).flushDuration;
        METRICS.S_BETA_FLUSH_TIMINGS[frameCount] = scheduler.timings(sBetaTimingHandle).flushDuration;
        METRICS.S_DELTA_DET_GO_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaDetGoTimingHandle).flushDuration;
        METRICS.S_DELTA_DET_PRT_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaDetPrtTimingHandle).flushDuration;
        METRICS.S_DELTA_RES_GO_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaResGoTimingHandle).flushDuration;
        METRICS.S_DELTA_RES_PRT_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaResPrtTimingHandle).flushDuration;
        METRICS.S_LAMBDA_FLUSH_TIMINGS[frameCount] = scheduler.timings(sLambdaTimingHandle).flushDuration;

        METRICS.P_DET_TIMINGS[frameCount] = scheduler.timings(pDetHandle).duration;
        METRICS.P_RES_TIMINGS[frameCount] = scheduler.timings(pResHandle).duration;


        METRICS.SCHEDULE_TIMINGS[frameCount] = (systemSchedule.timings()).total();

        #if !HEADLESS
            fpsMetrics.addFrame(frameTiming);
        #endif

        thesis::util::updateFrameMetrics(ecsDataContainer, frameTiming, frameCount, totalTime);
    }

    auto sysResultGo = helios::ecs::system::System(thesis::systems::CollectSpatialResultsSystem<GameObject>(
        METRICS.POSITION_GO, METRICS.VELOCITY_GO));
    sysResultGo.update(ecsDataContainer);
    auto sysResultPrt = helios::ecs::system::System(thesis::systems::CollectSpatialResultsSystem<Particle>(
        METRICS.POSITION_PRT, METRICS.VELOCITY_PRT));
    sysResultPrt.update(ecsDataContainer);

    thesis::util::writeMetricsToCSV(METRICS);

    return 0;
}