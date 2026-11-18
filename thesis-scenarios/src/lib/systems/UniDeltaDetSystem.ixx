/**
 * @file GridCollisionDetectionSystem.ixx
 * @brief Spatial-partitioning based collision detection using a uniform 3D grid.
 */
module;

#include <vector>
#include <cstddef>


export module thesis.systems.UniDeltaDetSystem;

import thesis.strategies;

import helios.physics.collision.components;
import helios.physics.collision.types;

import helios.engine.core.types;

import helios.ecs.entity;

import helios.core.thread;



import thesis.components;
import thesis.types;

import helios.engine.spatial.components;

export namespace thesis::systems {

    template<
        typename THandle,
        typename TStrategy = thesis::strategies::GridCollisionDetection<THandle>,
        bool TUseParallel = false
    >
    class UniDeltaDetSystem {

        using DetectionStrategy = TStrategy;

        using CollisionComponent = helios::physics::collision::components::CollisionComponent;
        using WorldBoundsComponent = helios::engine::spatial::components::BoundsComponent<helios::engine::core::types::World>;

        using CollisionResult =  helios::physics::collision::types::CollisionDetectionResult<THandle>;

        using QueryGo =  helios::ecs::entity::query::Query<
                THandle,
                helios::ecs::entity::ReadSet<
                    CollisionComponent,
                    WorldBoundsComponent,
                    thesis::components::GameObjectTagComponent
                >
            >;

        using QueryPrt = helios::ecs::entity::query::Query<
                THandle,
                helios::ecs::entity::ReadSet<
                    CollisionComponent,
                    WorldBoundsComponent,
                    thesis::components::ParticleTagComponent
                >
            >;
        DetectionStrategy strategyGo_;
        DetectionStrategy strategyPrt_;

        /**
         * Tracking of the executing thread per frame.
         */
        std::vector<std::size_t>* goThreadIds_ = nullptr;
        std::vector<std::size_t>* prtThreadIds_ = nullptr;
        const std::size_t* frame_ = nullptr;

        CollisionResult processGo(QueryGo& queryGo, helios::core::thread::ThreadPool& threadPool) {
            if (goThreadIds_ && frame_ && *frame_ < goThreadIds_->size()) {
                (*goThreadIds_)[*frame_] = helios::core::thread::ThreadPool::currentThreadId();
            }
            strategyGo_.reset();
            for (auto [entity, cc, wb, tag] : queryGo) {
                strategyGo_.addCollisionCandidate(entity.handle(), wb->value());
            }

            return strategyGo_.detectCollisions(threadPool);
        }

        CollisionResult processPrt(QueryPrt& queryPrt, helios::core::thread::ThreadPool& threadPool) {
            if (prtThreadIds_ && frame_ && *frame_ < prtThreadIds_->size()) {
                (*prtThreadIds_)[*frame_] = helios::core::thread::ThreadPool::currentThreadId();
            }
            strategyPrt_.reset();
            for (auto [entity, cc, wb, tag] : queryPrt) {
                strategyPrt_.addCollisionCandidate(entity.handle(), wb->value());
            }

            return strategyPrt_.detectCollisions(threadPool);
        }

    public:

        explicit UniDeltaDetSystem(TStrategy&& strategyGo, TStrategy&& strategyPrt)
        : strategyGo_(std::move(strategyGo)), strategyPrt_(std::move(strategyPrt)) {
        }

        /**
         * @param goThreadIds  Receives the id of the thread that processes Go in frame `frame`.
         * @param prtThreadIds Receives the id of the thread that processes Prt in frame `frame`.
         * @param frame        Reference to the current frame counter (must outlive the system).
         */
        UniDeltaDetSystem(
            TStrategy&& strategyGo, TStrategy&& strategyPrt,
            std::vector<std::size_t>& goThreadIds, std::vector<std::size_t>& prtThreadIds,
            const std::size_t& frame
        )
        : strategyGo_(std::move(strategyGo)), strategyPrt_(std::move(strategyPrt)),
          goThreadIds_(&goThreadIds), prtThreadIds_(&prtThreadIds), frame_(&frame) {
        }


        thesis::types::UniCollisionDetectionResult<THandle> update(
            QueryGo queryGo,
            QueryPrt queryPrt,
            helios::core::thread::ThreadPool& threadPool
        ) noexcept {

            if constexpr (TUseParallel) {

                CollisionResult resultGo;
                CollisionResult resultPrt;

                threadPool.runAndWait(2, [&](const std::size_t taskIndex) {
                    if (taskIndex == 0) {
                        resultGo = processGo(queryGo, threadPool);
                    } else {
                        resultPrt = processPrt(queryPrt, threadPool);
                    }

                });

                return thesis::types::UniCollisionDetectionResult<THandle>(
                    std::move(resultGo),
                    std::move(resultPrt)
                );

            } else {
                return thesis::types::UniCollisionDetectionResult<THandle>(
                   std::move(processGo(queryGo, threadPool)),
                   std::move(processPrt(queryPrt, threadPool))
               );
            }

        }



    };


}
