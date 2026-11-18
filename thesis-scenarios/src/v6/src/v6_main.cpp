
#include <string>
#include "helios-ecs-config.h"


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
import thesis.bootstrap;
import thesis.config;
import thesis.util;
import thesis.systems;
import thesis.manager;
import thesis.types;
import thesis.components;


#include "../../lib/include/Namespaces.h"

using namespace thesis::config;

#ifndef HEADLESS
#define HEADLESS false
#endif

int main(int argc, char** argv) {

    std::string PROGRAM_ID = "v3";

    #include "../../lib/include/Setup.inc"
    #include "../../lib/include/RenderSetup.inc"

    using DeltaManager = thesis::manager::TaggedDeltaManager<UniHandle, UniHandle, thesis::components::GameObjectTagComponent>;
    using LambdaManager = thesis::manager::LambdaManager<UniHandle>;
    using BetaSystem = thesis::systems::BetaSystem<UniHandle>;
    using DeltaDetSystemGo = thesis::systems::TaggedDeltaDetSystem<UniHandle, thesis::components::GameObjectTagComponent>;
    using DeltaDetSystemPrt = thesis::systems::TaggedDeltaDetSystem<UniHandle, thesis::components::ParticleTagComponent>;
    using DeltaResSystemGo = thesis::systems::TaggedDeltaResSystemV6<UniHandle, thesis::components::GameObjectTagComponent>;
    using DeltaResSystemPrt = thesis::systems::TaggedDeltaResSystemV6<UniHandle, thesis::components::ParticleTagComponent, false>;
    using LambdaSystem = thesis::systems::LambdaSystem<UniHandle>;

    using DeltaDetResultGo = thesis::types::TaggedDeltaDetResult<UniHandle, thesis::components::GameObjectTagComponent>;
    using DeltaResResultGo = thesis::types::TaggedDeltaResResult<UniHandle, thesis::components::GameObjectTagComponent>;

    using DeltaDetResultPrt = thesis::types::TaggedDeltaDetResult<UniHandle, thesis::components::ParticleTagComponent>;
    using DeltaResResultPrt = thesis::types::TaggedDeltaResResult<UniHandle, thesis::components::ParticleTagComponent>;


    #if HEADLESS
        auto gameObjectRenderCallback = [&](auto& gameObject) {
            gameObject.template add<thesis::components::GameObjectTagComponent>();
        };
        auto borderRenderCallbackGameObject = [&](auto& border) {
            border.template add<thesis::components::GameObjectTagComponent>();
        };
        auto borderRenderCallbackParticle = [&](auto& border) {
            border.template add<thesis::components::ParticleTagComponent>();
        };
        auto particleRenderCallback = [&](auto& particle) {
            particle.template add<thesis::components::ParticleTagComponent>();
        };

    #else
        auto gameObjectRenderCallback = [&](auto& gameObject) {
            gameObject.setActive(true);
            gameObject.template add<thesis::components::GameObjectTagComponent>();
            gameObject.template add<DefaultSceneMemberComponent>(MainScene);
            gameObject.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), GameObjectMaterial.handle(), RectMesh.handle()
            );
        };

        auto borderRenderCallbackGameObject = [&](auto& border) {
            border.setActive(true);
            border.template add<thesis::components::GameObjectTagComponent>();
            border.template add<DefaultSceneMemberComponent>(MainScene);
            border.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), BoundaryMaterial.handle(), RectMesh.handle()
            );
        };

        auto borderRenderCallbackParticle = [&](auto& border) {
            border.setActive(true);
            border.template add<thesis::components::ParticleTagComponent>();
            border.template add<DefaultSceneMemberComponent>(MainScene);
            border.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), BoundaryMaterial.handle(), RectMesh.handle()
            );
        };

        auto particleRenderCallback = [&](auto& particle) {
            particle.setActive(true);
            particle.template add<thesis::components::ParticleTagComponent>();
            particle.template add<DefaultSceneMemberComponent>(MainScene);
            particle.template add<DefaultRenderPrototypeComponent<Instanced>>(
                InstanceShader.handle(), ParticleMaterial.handle(), RectMesh.handle()
            );
        };
    #endif

    auto collisionCallback = thesis::util::collCb<UniHandle>(particleRenderCallback);

    auto m_delta             = DeltaManager{collisionCallback};
    auto m_lambda             = LambdaManager();
    auto s_beta            = BetaSystem();
    auto s_delta_det_go    = DeltaDetSystemGo(thesis::util::grid<UniHandle>());
    auto s_delta_res_go    = DeltaResSystemGo();
    auto s_delta_det_prt   = DeltaDetSystemPrt(thesis::util::grid<UniHandle>());
    auto s_delta_res_prt   = DeltaResSystemPrt();
    auto s_lambda          = LambdaSystem();

    #include "../../lib/include/ToolingSetup.inc"
    #include "../../lib/include/SchedulerPreSetup.inc"

    thesis::util::createGameObjects<UniHandle>(
        ecsDataContainer.get<EntityManager<UniHandle>>(),
        gameObjectRenderCallback
    );
    thesis::util::createBoundaries<UniHandle>(
        ecsDataContainer.get<EntityManager<UniHandle>>(),
        borderRenderCallbackGameObject
    );
    thesis::util::createBoundaries<UniHandle>(
        ecsDataContainer.get<EntityManager<UniHandle>>(),
        borderRenderCallbackParticle
    );

    // +----------------------------------------
    // | THESIS SCHEDULE
    // +----------------------------------------
    systemSchedule
        .startWith(m_delta)
            .add(
                Sequential(
                    s_beta,
                    s_delta_det_go, s_delta_res_go,
                    s_delta_det_prt, s_delta_res_prt,
                    s_lambda
                )
            )
        .endWith(m_lambda);
    // +----------------------------------------
    // +----------------------------------------

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
        systemSchedule.idx(), 0, 0, 1,
    };

    auto sDeltaResGoTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaResSystemGo>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 2,
    };

    auto sDeltaDetPrtTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaDetSystemPrt>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 3,
    };

    auto sDeltaResPrtTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaResSystemPrt>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 4,
    };

    auto sLambdaTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<LambdaSystem>(),
        helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 5,
    };

    #if !HEADLESS
    scheduler.beginSchedule(GameWorld::sessionState(EngineState::Running))
            .add(
                Sequential(
                    SceneMemberVisibilitySystem<
                        UniHandle, Instanced, NullCullingStrategy<UniHandle>, DefaultRenderHandles
                    >(NullCullingStrategy<UniHandle>()),
                    [&](EntityManager<UniHandle>& entityManager,
                       DefaultSceneMemberVisibilityRegistry<UniHandle, Instanced>& visibilityRegistry) {
                        // only entities with a LifetimeComponent (particles) fade with their age
                        auto* lifetimeSparseSet = entityManager.sparseSet<LifetimeComponent>();
                        visibilityRegistry.forEachVisibleMember([&lifetimeSparseSet = *lifetimeSparseSet](auto& visibleContext) {
                            if (auto* cmp = lifetimeSparseSet.get(visibleContext.memberHandle.entityId())) {
                                visibleContext.normalizedAge = cmp->normalizedAge();
                            }
                        });
                    },
                    DefaultSceneRenderSystem<UniHandle, Instanced>()
                )
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

        auto deltaDetResultGo = rrr.item<DeltaDetResultGo>();
        auto deltaResResultGo = rrr.item<DeltaResResultGo>();
        auto deltaDetResultPrt = rrr.item<DeltaDetResultPrt>();
        auto deltaResResultPrt = rrr.item<DeltaResResultPrt>();

        METRICS.COLLISION_PAIRS_FOUND_GO[frameCount] = deltaDetResultGo
            ? deltaDetResultGo->collisionPairs().size() : 0;
        METRICS.COLLISION_PAIRS_FOUND_PRT[frameCount] = deltaDetResultPrt
            ? deltaDetResultPrt->collisionPairs().size() : 0;

        METRICS.COLLISION_PAIRS_PROCESSED_GO[frameCount] = deltaResResultGo
            ? deltaResResultGo->processedCollisions : 0;
        METRICS.COLLISION_PAIRS_PROCESSED_PRT[frameCount] = deltaResResultPrt
            ? deltaResResultPrt->processedCollisions : 0;

        METRICS.PARTICLE_POPULATION[frameCount] = ecsDataContainer.get<EntityManager<UniHandle>>()
        .sparseSet<thesis::components::ParticleTagComponent>()->componentCount();

        METRICS.M_COL_TIMINGS[frameCount] = scheduler.timings(mColTimingHandle).updateDuration;
        METRICS.M_LFT_TIMINGS[frameCount] = scheduler.timings(mLftTimingHandle).updateDuration;
        METRICS.S_BETA_TIMINGS[frameCount] = scheduler.timings(sBetaTimingHandle).updateDuration;
        METRICS.S_DELTA_DET_GO_TIMINGS[frameCount] = scheduler.timings(sDeltaDetGoTimingHandle).updateDuration;
        METRICS.S_DELTA_RES_GO_TIMINGS[frameCount] = scheduler.timings(sDeltaResGoTimingHandle).updateDuration;
        METRICS.S_DELTA_DET_PRT_TIMINGS[frameCount] = scheduler.timings(sDeltaDetPrtTimingHandle).updateDuration;
        METRICS.S_DELTA_RES_PRT_TIMINGS[frameCount] = scheduler.timings(sDeltaResPrtTimingHandle).updateDuration;

        METRICS.M_COL_THREAD_IDS[frameCount] = scheduler.timings(mColTimingHandle).threadId;
        METRICS.M_LFT_THREAD_IDS[frameCount] = scheduler.timings(mLftTimingHandle).threadId;
        METRICS.S_BETA_THREAD_IDS[frameCount] = scheduler.timings(sBetaTimingHandle).threadId;
        METRICS.S_DELTA_DET_GO_THREAD_IDS[frameCount] = scheduler.timings(sDeltaDetGoTimingHandle).threadId;
        METRICS.S_DELTA_RES_GO_THREAD_IDS[frameCount] = scheduler.timings(sDeltaResGoTimingHandle).threadId;
        METRICS.S_DELTA_DET_PRT_THREAD_IDS[frameCount] = scheduler.timings(sDeltaDetPrtTimingHandle).threadId;
        METRICS.S_DELTA_RES_PRT_THREAD_IDS[frameCount] = scheduler.timings(sDeltaResPrtTimingHandle).threadId;

        METRICS.M_COL_FLUSH_TIMINGS[frameCount] = scheduler.timings(mColTimingHandle).flushDuration;
        METRICS.M_LFT_FLUSH_TIMINGS[frameCount] = scheduler.timings(mLftTimingHandle).flushDuration;
        METRICS.S_BETA_FLUSH_TIMINGS[frameCount] = scheduler.timings(sBetaTimingHandle).flushDuration;
        METRICS.S_DELTA_DET_GO_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaDetGoTimingHandle).flushDuration;
        METRICS.S_DELTA_RES_GO_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaResGoTimingHandle).flushDuration;
        METRICS.S_DELTA_DET_PRT_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaDetPrtTimingHandle).flushDuration;
        METRICS.S_DELTA_RES_PRT_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaResPrtTimingHandle).flushDuration;
        METRICS.S_LAMBDA_FLUSH_TIMINGS[frameCount] = scheduler.timings(sLambdaTimingHandle).flushDuration;


        METRICS.P_DET_TIMINGS[frameCount] = METRICS.S_DELTA_DET_GO_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_DET_PRT_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_DET_PRT_FLUSH_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_DET_GO_FLUSH_TIMINGS[frameCount];

         METRICS.P_RES_TIMINGS[frameCount] = METRICS.S_DELTA_RES_GO_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_RES_PRT_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_RES_PRT_FLUSH_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_RES_GO_FLUSH_TIMINGS[frameCount];


        METRICS.S_LAMBDA_TIMINGS[frameCount] = scheduler.timings(sLambdaTimingHandle).updateDuration;

        METRICS.SCHEDULE_TIMINGS[frameCount] = (systemSchedule.timings()).total();

        #if !HEADLESS
            fpsMetrics.addFrame(frameTiming);
        #endif

        thesis::util::updateFrameMetrics(ecsDataContainer, frameTiming, frameCount, totalTime);
    }

    auto sys = helios::ecs::system::System(
        thesis::systems::CollectTaggedSpatialResultsSystem<UniHandle>(METRICS)
    );
    sys.update(ecsDataContainer);

    thesis::util::writeMetricsToCSV(METRICS);

    return 0;
}