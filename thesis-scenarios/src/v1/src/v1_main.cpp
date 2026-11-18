
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

    std::string PROGRAM_ID = "v1";

    #include "../../lib/include/Setup.inc"
    #include "../../lib/include/RenderSetup.inc"

    using DeltaManager = thesis::manager::DeltaManager<UniHandle, UniHandle>;
    using LambdaManager = thesis::manager::LambdaManager<UniHandle>;
    using BetaSystem = thesis::systems::BetaSystem<UniHandle>;
    using DeltaDetSystem = thesis::systems::UniDeltaDetSystem<UniHandle>;
    using DeltaResSystem = thesis::systems::UniDeltaResSystem<UniHandle>;
    using LambdaSystem = thesis::systems::LambdaSystem<UniHandle>;

    using CollisionDetectionResult = thesis::types::UniCollisionDetectionResult<UniHandle>;
    using DeltaResResult = thesis::types::UniDeltaResResult<UniHandle>;

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
    auto s_delta_det       = DeltaDetSystem(thesis::util::grid<UniHandle>(), thesis::util::grid<UniHandle>());
    auto s_delta_res       = DeltaResSystem();
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
                    s_beta, s_delta_det, s_delta_res, s_lambda
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

    auto sDeltaDetTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaDetSystem>(),
         helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 1,
    };

    auto sDeltaResTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<DeltaResSystem>(),
         helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 2,
    };

    auto sLambdaTimingHandle = helios::ecs::scheduling::types::RuntimeSystemEntryHandle<helios::ecs::system::types::SystemTypeId>{
        helios::ecs::system::types::SystemTypeId::template id<LambdaSystem>(),
         helios::ecs::scheduling::types::SchedulePhase::System,
        systemSchedule.idx(), 0, 0, 3,
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

        auto collisionDetectionResult = rrr.item<CollisionDetectionResult>();

        METRICS.COLLISION_PAIRS_FOUND_GO[frameCount] = collisionDetectionResult
            ? collisionDetectionResult->collisionPairsGo().size() : 0;
        METRICS.COLLISION_PAIRS_FOUND_PRT[frameCount] = collisionDetectionResult
            ? collisionDetectionResult->collisionPairsPrt().size() : 0;

        auto collisionProcessedResult = rrr.item<DeltaResResult>();
        METRICS.COLLISION_PAIRS_PROCESSED_GO[frameCount] = collisionProcessedResult
            ? collisionProcessedResult->processedCollisionsGo : 0;
        METRICS.COLLISION_PAIRS_PROCESSED_PRT[frameCount] = collisionProcessedResult
            ? collisionProcessedResult->processedCollisionsPrt : 0;

        METRICS.PARTICLE_POPULATION[frameCount] = ecsDataContainer.get<EntityManager<UniHandle>>()
        .sparseSet<thesis::components::ParticleTagComponent>()->componentCount();

        METRICS.M_COL_TIMINGS[frameCount] = scheduler.timings(mColTimingHandle).updateDuration;
        METRICS.M_LFT_TIMINGS[frameCount] = scheduler.timings(mLftTimingHandle).updateDuration;
        METRICS.S_BETA_TIMINGS[frameCount] = scheduler.timings(sBetaTimingHandle).updateDuration;
        METRICS.S_DELTA_DET_GO_TIMINGS[frameCount] = scheduler.timings(sDeltaDetTimingHandle).updateDuration;
        METRICS.S_DELTA_RES_GO_TIMINGS[frameCount] = scheduler.timings(sDeltaResTimingHandle).updateDuration;
        METRICS.S_LAMBDA_TIMINGS[frameCount] = scheduler.timings(sLambdaTimingHandle).updateDuration;

        METRICS.M_COL_THREAD_IDS[frameCount] = scheduler.timings(mColTimingHandle).threadId;
        METRICS.M_LFT_THREAD_IDS[frameCount] = scheduler.timings(mLftTimingHandle).threadId;
        METRICS.S_BETA_THREAD_IDS[frameCount] = scheduler.timings(sBetaTimingHandle).threadId;
        METRICS.S_DELTA_DET_GO_THREAD_IDS[frameCount] = scheduler.timings(sDeltaDetTimingHandle).threadId;
        METRICS.S_DELTA_RES_GO_THREAD_IDS[frameCount] = scheduler.timings(sDeltaResTimingHandle).threadId;
        METRICS.S_LAMBDA_THREAD_IDS[frameCount] = scheduler.timings(sLambdaTimingHandle).threadId;

        METRICS.M_COL_FLUSH_TIMINGS[frameCount] = scheduler.timings(mColTimingHandle).flushDuration;
        METRICS.M_LFT_FLUSH_TIMINGS[frameCount] = scheduler.timings(mLftTimingHandle).flushDuration;
        METRICS.S_BETA_FLUSH_TIMINGS[frameCount] = scheduler.timings(sBetaTimingHandle).flushDuration;
        METRICS.S_DELTA_DET_GO_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaDetTimingHandle).flushDuration;
        METRICS.S_DELTA_RES_GO_FLUSH_TIMINGS[frameCount] = scheduler.timings(sDeltaResTimingHandle).flushDuration;
        METRICS.S_LAMBDA_FLUSH_TIMINGS[frameCount] = scheduler.timings(sLambdaTimingHandle).flushDuration;

        METRICS.SCHEDULE_TIMINGS[frameCount] = (systemSchedule.timings()).total();

        METRICS.P_DET_TIMINGS[frameCount] = METRICS.S_DELTA_DET_GO_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_DET_GO_FLUSH_TIMINGS[frameCount];

         METRICS.P_RES_TIMINGS[frameCount] = METRICS.S_DELTA_RES_GO_TIMINGS[frameCount] +
                                            METRICS.S_DELTA_RES_GO_FLUSH_TIMINGS[frameCount];


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