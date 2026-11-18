
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

    std::string PROGRAM_ID = "v4";

    #include "../../lib/include/Setup.inc"


    using DeltaManager = thesis::manager::TaggedDeltaManager<UniHandle, UniHandle, thesis::components::GameObjectTagComponent>;
    using LambdaManager = thesis::manager::LambdaManager<UniHandle>;
    using BetaSystem = thesis::systems::BetaSystem<UniHandle>;
    using DeltaDetSystemGo = thesis::systems::TaggedDeltaDetSystem<UniHandle, thesis::components::GameObjectTagComponent>;
    using DeltaDetSystemPrt = thesis::systems::TaggedDeltaDetSystem<UniHandle, thesis::components::ParticleTagComponent>;
    using DeltaResSystemGo = thesis::systems::TaggedDeltaResSystem<UniHandle, thesis::components::GameObjectTagComponent>;
    using DeltaResSystemPrt = thesis::systems::TaggedDeltaResSystem<UniHandle, thesis::components::ParticleTagComponent, false>;
    using LambdaSystem = thesis::systems::LambdaSystem<UniHandle>;

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

    auto collisionCallback = thesis::util::collCb<UniHandle>(particleRenderCallback);

    auto m_delta            = DeltaManager{collisionCallback};
    auto m_lambda           = LambdaManager();
    auto s_beta            = BetaSystem();
    auto s_delta_det_go    = DeltaDetSystemGo(thesis::util::grid<UniHandle>());
    auto s_delta_res_go    = DeltaResSystemGo();
    auto s_delta_det_prt   = DeltaDetSystemPrt(thesis::util::grid<UniHandle>());
    auto s_delta_res_prt   = DeltaResSystemPrt();
    auto s_lambda          = LambdaSystem();

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

    auto& systemSchedule = scheduler.beginSchedule();

    systemSchedule
        .startWith(m_delta)
            .add(s_beta)
            .add(s_delta_det_go, s_delta_det_prt)
            .add(s_delta_res_go, s_delta_res_prt)
            .add(s_lambda)
        .endWith(m_lambda);
    // +----------------------------------------
    // +----------------------------------------

    thesis::util::init(ecsDataContainer);

    while (true) {

        scheduler.update();
    }


    return 0;
}