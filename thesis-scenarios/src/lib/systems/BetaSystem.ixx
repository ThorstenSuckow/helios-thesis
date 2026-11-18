/**
 * @file BetaSystem.ixx
 * @brief Integrates local position using local velocity and frame delta time.
 */
module;

export module thesis.systems.BetaSystem;

import helios.physics.motion.components;

import helios.ecs.entity.QueryAccessSet;
import helios.ecs.entity.query.Query;

import helios.math;
import helios.engine.core.types;
import helios.engine.runtime.gameloop.types;
import helios.engine.spatial.components;

#ifndef HEADLESS
    #define HEADLESS false
#endif

export namespace thesis::systems {

    template<typename... TMemberHandles>
    class BetaSystem {

        using UpdateContext = helios::engine::runtime::gameloop::types::UpdateContext;

        using Local = helios::engine::core::types::Local;
        using World = helios::engine::core::types::World;

        template<typename... TReads>
        using Read = helios::ecs::entity::ReadSet<TReads...>;

        template<typename... TWrites>
        using Write = helios::ecs::entity::WriteSet<TWrites...>;

        template<typename TMemberHandle>
        using Query = helios::ecs::entity::query::Query<
            TMemberHandle,
            Read<
                helios::physics::motion::components::Velocity3DComponent<Local>,
                helios::engine::spatial::components::Position3DComponent<Local>,
                helios::engine::spatial::components::BoundsComponent<Local>,
                helios::engine::spatial::components::BoundsComponent<World>
#if !HEADLESS
                ,helios::engine::spatial::components::TransformComponent<World>
#endif
            >,
            Write<
                helios::engine::spatial::components::Position3DComponent<Local>,
                helios::engine::spatial::components::BoundsComponent<World>
#if !HEADLESS
                , helios::engine::spatial::components::TransformComponent<World>
#endif
            >
        >;

        template<typename TMemberHandle>
        static void process(
            Query<TMemberHandle> query,
            const UpdateContext& updateContext
        ) noexcept {

            for (auto [
                entity,
                localVelocity,
                localPosition,
                boundsLocal,
                boundsWorld
        #if !HEADLESS
            , worldTransform
        #endif
            ] : query) {

                const auto position =
                    localPosition->value()
                    + localVelocity->value() * updateContext.deltaTime();

                entity.template track<
                    helios::engine::spatial::components::Position3DComponent<Local>
                >()->setValue(position);


#if !HEADLESS
                const auto transform =
                    helios::math::mat4f::identity().withTranslation(position);
                entity.template track<
                    helios::engine::spatial::components::TransformComponent<World>
                >()->setValue(transform);
#endif

                // matrix computation not necessary when in HEADLESS mode
                entity.template track<
                    helios::engine::spatial::components::BoundsComponent<World>
                >()->setValue(boundsLocal->value() + position);
            }
        }

    public:

        /**
         * @brief Updates entities of all member handle types sequentially,
         * in the order of the template arguments.
         */
        void update(
            Query<TMemberHandles>... queries,
            const UpdateContext& updateContext
        ) noexcept {
            (process<TMemberHandles>(queries, updateContext), ...);
        }
    };

}