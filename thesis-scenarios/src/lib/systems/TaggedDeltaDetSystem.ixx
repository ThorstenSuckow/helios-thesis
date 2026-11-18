/**
 * @file GridCollisionDetectionSystem.ixx
 * @brief Spatial-partitioning based collision detection using a uniform 3D grid.
 */
module;

#include <utility>


export module thesis.systems.TaggedDeltaDetSystem;

import thesis.strategies;
import thesis.types;

import helios.physics.collision.components;

import helios.engine.core.types;

import helios.ecs.entity;

import helios.core.thread;



import helios.engine.spatial.components;

export namespace thesis::systems {

    template<
        typename THandle,
        typename TComponentTag,
        typename TStrategy = thesis::strategies::GridCollisionDetection<THandle>
    >
    class TaggedDeltaDetSystem {

        using DetectionStrategy = TStrategy;

        using CollisionComponent = helios::physics::collision::components::CollisionComponent;
        using WorldBoundsComponent = helios::engine::spatial::components::BoundsComponent<helios::engine::core::types::World>;

        DetectionStrategy strategy_;

    public:



        explicit TaggedDeltaDetSystem(TStrategy&& strategy)
        : strategy_(std::move(strategy)) {
        }


        thesis::types::TaggedDeltaDetResult<THandle, TComponentTag> update(
            helios::ecs::entity::query::Query<
                THandle,
                helios::ecs::entity::ReadSet<
                    CollisionComponent,
                    WorldBoundsComponent,
                    TComponentTag
                >
            > query,
            helios::core::thread::ThreadPool& threadPool
        ) noexcept {

            strategy_.reset();

            for (auto [entity, cc, wb, tag] : query) {
                strategy_.addCollisionCandidate(entity.handle(), wb->value());
            }

            return thesis::types::TaggedDeltaDetResult<THandle, TComponentTag>(
                strategy_.detectCollisions(threadPool)
            );
        }



    };


}
