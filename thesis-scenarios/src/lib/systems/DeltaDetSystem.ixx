/**
 * @file GridCollisionDetectionSystem.ixx
 * @brief Spatial-partitioning based collision detection using a uniform 3D grid.
 */
module;

#include <utility>


export module thesis.systems.DeltaDetSystem;

import thesis.strategies;

import helios.physics.collision.components;
import helios.physics.collision.types;

import helios.engine.core.types;

import helios.ecs.entity;

import helios.core.thread;



import helios.engine.spatial.components;

export namespace thesis::systems {

    template<
        typename THandle,
        typename TStrategy = thesis::strategies::GridCollisionDetection<THandle>
    >
    class DeltaDetSystem {

        using DetectionStrategy = TStrategy;

        using CollisionComponent = helios::physics::collision::components::CollisionComponent;
        using WorldBoundsComponent = helios::engine::spatial::components::BoundsComponent<helios::engine::core::types::World>;

        DetectionStrategy strategy_;

    public:



        explicit DeltaDetSystem(TStrategy&& strategy)
        : strategy_(std::move(strategy)) {
        }


        helios::physics::collision::types::CollisionDetectionResult<THandle> update(
            helios::ecs::entity::query::Query<
                THandle,
                helios::ecs::entity::ReadSet<
                    CollisionComponent,
                    WorldBoundsComponent
                >
            > query,
            helios::core::thread::ThreadPool& threadPool
        ) noexcept {

            strategy_.reset();

            for (auto [entity, cc, wb] : query) {
                strategy_.addCollisionCandidate(entity.handle(), wb->value());
            }

            return strategy_.detectCollisions(threadPool);
        }



    };


}
