
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

    std::string PROGRAM_ID = "v8";


    #include "../../lib/include/Setup.inc"

    #include "../../lib/include/RenderSetup.inc"


    using GameObject = GameObjectHandle;
    using Particle = ParticleHandle;

    using DeltaManager = thesis::manager::DeltaManager<GameObject, Particle>;
    using LambdaManager = thesis::manager::LambdaManager<Particle>;
    using BetaSystemGo = thesis::systems::BetaSystem<GameObject>;
    using BetaSystemPrt = thesis::systems::BetaSystem<Particle>;
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

    auto m_delta           = DeltaManager{collisionCallback};
    auto m_lambda          = LambdaManager();
    auto s_beta_go         = BetaSystemGo();
    auto s_beta_prt        = BetaSystemPrt();
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
            .add(
                // GameObject sequential group
                Sequential(
                    s_beta_go, s_delta_det_go, s_delta_res_go
                ),
                // Particle sequential group
                Sequential(
                    s_beta_prt, s_delta_det_prt, s_delta_res_prt, s_lambda
                )
            )
        .endWith(m_lambda);
    // +----------------------------------------
    // +----------------------------------------


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