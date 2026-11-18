/**
* @file CollisionResponseSystem.ixx
 * @brief Applies collision response functions sequentially to collision pairs.
 */
module;

#include <cstddef>
#include <vector>
#include <span>
#include <utility>

export module thesis.systems.UniDeltaResSystem;

import thesis.strategies;

import thesis.types;

import helios.core.thread;

import helios.physics.collision.types;
import helios.physics.motion.components;

import helios.engine.core.types;

import helios.ecs.entity;

import helios.math;


import helios.physics.collision.commands;

import helios.ecs.command.TypedCommandBuffer;

export namespace thesis::systems {


    template<
        typename THandle,
        typename TResponseStrategy = thesis::strategies::CollisionImpulseResponse,
        bool TUseParallel = false
    >
    class UniDeltaResSystem {

        using ResponseStrategy = TResponseStrategy;

        using CollisionCommand = helios::physics::collision::commands::CollisionCommand<THandle>;

        using CommandBuffer = helios::ecs::command::TypedCommandBuffer<CollisionCommand>;

        using Query = helios::ecs::entity::query::Query<
            THandle,
            helios::ecs::entity::ReadSet<helios::physics::motion::components::Velocity3DComponent<helios::engine::core::types::Local>>,
            helios::ecs::entity::WriteSet<helios::physics::motion::components::Velocity3DComponent<helios::engine::core::types::Local>>
        >;

        ResponseStrategy responseStrategy_;

        /**
         * Optional tracking of the executing thread per frame (see UniDeltaDetSystem).
         */
        std::vector<std::size_t>* goThreadIds_ = nullptr;
        std::vector<std::size_t>* prtThreadIds_ = nullptr;
        const std::size_t* frame_ = nullptr;

        std::size_t processCollisions(
            std::span<const helios::physics::collision::types::CollisionPair<THandle>> collisionPairs,
            Query& query,
            CommandBuffer* commandBuffer = nullptr
        ) noexcept {

            std::size_t processedCollisions = 0;

            for (const auto& collisionPair : collisionPairs) {

                auto lftResult = query.get(collisionPair.leftHandle);
                auto rgtResult = query.get(collisionPair.rightHandle);

                // stationary collision (most unlikely)
                if (!lftResult && !rgtResult) [[unlikely]] {
                    continue;
                }

                helios::math::vec3f leftVelocity{0.0f, 0.0f, 0.0f};
                helios::math::vec3f rightVelocity{0.0f, 0.0f, 0.0f};

                if (lftResult) {
                    auto [entity, velocity] = *lftResult;
                    leftVelocity = velocity->value();
                }

                if (rgtResult) {
                    auto [entity, velocity] = *rgtResult;
                    rightVelocity = velocity->value();
                }

                auto collisionSnapshot = collisionPair.collisionSnapshot;
                collisionSnapshot.leftDynamic = static_cast<bool>(lftResult);
                collisionSnapshot.rightDynamic = static_cast<bool>(rgtResult);

                auto collisionResponse = responseStrategy_.process(
                    collisionSnapshot,
                    leftVelocity,
                    rightVelocity
                );

                if (!collisionResponse) {
                    continue;
                }

                leftVelocity = collisionResponse->leftVelocity;
                rightVelocity = collisionResponse->rightVelocity;

                if (lftResult) {
                    auto [entity, velocity] = *lftResult;

                    velocity->setValue(
                        leftVelocity
                    );
                }

                if (rgtResult) {
                    auto [entity, velocity] = *rgtResult;
                    velocity->setValue(rightVelocity);
                }

                processedCollisions++;

                if (commandBuffer) {
                    commandBuffer->template add<CollisionCommand>(CollisionCommand{
                        .leftHandle = collisionPair.leftHandle,
                        .rightHandle = collisionPair.rightHandle,
                        .type = helios::physics::collision::commands::CollisionCommandType::Impact,
                        .location = collisionPair.collisionSnapshot.overlapCenter
                    });
                }
            }
            return processedCollisions;
        }

    public:

        UniDeltaResSystem() = default;

        explicit UniDeltaResSystem(TResponseStrategy&& responseStrategy)
            : responseStrategy_(std::move(responseStrategy)) {
        }

        /**
         * @param goThreadIds  Receives the id of the thread that processes Go in frame `frame`.
         * @param prtThreadIds Receives the id of the thread that processes Prt in frame `frame`.
         * @param frame        Reference to the current frame counter (must outlive the system).
         */
        UniDeltaResSystem(
            TResponseStrategy&& responseStrategy,
            std::vector<std::size_t>& goThreadIds, std::vector<std::size_t>& prtThreadIds,
            const std::size_t& frame
        )
            : responseStrategy_(std::move(responseStrategy)),
              goThreadIds_(&goThreadIds), prtThreadIds_(&prtThreadIds), frame_(&frame) {
        }

        thesis::types::UniDeltaResResult<THandle> update(
            CommandBuffer& commandBuffer,
            const thesis::types::UniCollisionDetectionResult<THandle>& collisionResult,
            Query query,
            helios::core::thread::ThreadPool& threadPool

        ) noexcept {
            std::size_t processedCollisionsGo = 0;
            std::size_t processedCollisionsPrt = 0;

            if constexpr (TUseParallel) {

                threadPool.runAndWait(2, [&](const std::size_t taskIndex) {
                    if (taskIndex == 0) {
                        if (goThreadIds_ && frame_ && *frame_ < goThreadIds_->size()) {
                            (*goThreadIds_)[*frame_] = helios::core::thread::ThreadPool::currentThreadId();
                        }
                        processedCollisionsGo = processCollisions(collisionResult.collisionPairsGo(),
                            query,
                            &commandBuffer);
                    } else {
                        if (prtThreadIds_ && frame_ && *frame_ < prtThreadIds_->size()) {
                            (*prtThreadIds_)[*frame_] = helios::core::thread::ThreadPool::currentThreadId();
                        }
                        processedCollisionsPrt = processCollisions(collisionResult.collisionPairsPrt(),
                            query);
                    }
                });

            } else {
                processedCollisionsGo = processCollisions(collisionResult.collisionPairsGo(), query, &commandBuffer);
                processedCollisionsPrt = processCollisions(collisionResult.collisionPairsPrt(), query);
            }

            return thesis::types::UniDeltaResResult<THandle>{processedCollisionsGo, processedCollisionsPrt};
        }
    };
}