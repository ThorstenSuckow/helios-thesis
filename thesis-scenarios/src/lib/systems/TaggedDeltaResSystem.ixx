/**
* @file CollisionResponseSystem.ixx
 * @brief Applies collision response functions sequentially to collision pairs.
 */
module;

#include <utility>

export module thesis.systems.TaggedDeltaResSystem;

import thesis.strategies;

import thesis.types;
import thesis.commands;

import helios.physics.motion.components;

import helios.engine.core.types;

import helios.ecs.entity;

import helios.math;


import helios.ecs.command.TypedCommandBuffer;

export namespace thesis::systems {


    template<
        typename THandle,
        typename TComponentTag,
        bool TCreateCommands = true,
        typename TResponseStrategy = thesis::strategies::CollisionImpulseResponse
    >
    class TaggedDeltaResSystem {

        using ResponseStrategy = TResponseStrategy;

        using CollisionCommand = thesis::commands::TaggedCollisionCommand<THandle, TComponentTag>;

        using CommandBuffer = helios::ecs::command::TypedCommandBuffer<CollisionCommand>;

        ResponseStrategy responseStrategy_;

    public:

        TaggedDeltaResSystem() = default;

        explicit TaggedDeltaResSystem(TResponseStrategy&& responseStrategy)
            : responseStrategy_(std::move(responseStrategy)) {
        }


         types::TaggedDeltaResResult<THandle, TComponentTag> update(
            CommandBuffer& commandBuffer,
            const thesis::types::TaggedDeltaDetResult<THandle, TComponentTag>& collisionResult,
            helios::ecs::entity::query::Query<
                THandle,
                helios::ecs::entity::ReadSet<
                    helios::physics::motion::components::Velocity3DComponent<helios::engine::core::types::Local>,
                    TComponentTag
                >,
                helios::ecs::entity::WriteSet<helios::physics::motion::components::Velocity3DComponent<helios::engine::core::types::Local>>
            > query

        ) noexcept {

            const auto& collisions = collisionResult.collisionPairs();

            std::size_t processedCollisions = 0;

            for (const auto& collisionPair : collisions) {

                auto lftResult = query.get(collisionPair.leftHandle);
                auto rgtResult = query.get(collisionPair.rightHandle);

                // stationary collision (most unlikely)
                if (!lftResult && !rgtResult) [[unlikely]] {
                    continue;
                }

                helios::math::vec3f leftVelocity{0.0f, 0.0f, 0.0f};
                helios::math::vec3f rightVelocity{0.0f, 0.0f, 0.0f};

                if (lftResult) {
                    auto [entity, velocity, tag] = *lftResult;
                    leftVelocity = velocity->value();
                }

                if (rgtResult) {
                    auto [entity, velocity, tag] = *rgtResult;
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
                    auto [entity, velocity, tag] = *lftResult;

                    velocity->setValue(
                        leftVelocity
                    );
                }

                if (rgtResult) {
                    auto [entity, velocity, tag] = *rgtResult;
                    velocity->setValue(rightVelocity);
                }

                processedCollisions++;

                if constexpr (TCreateCommands) {
                    commandBuffer.template add<CollisionCommand>(CollisionCommand{
                        .leftHandle = collisionPair.leftHandle,
                        .rightHandle = collisionPair.rightHandle,
                        .type = thesis::commands::CollisionCommandType::Impact,
                        .location = collisionPair.collisionSnapshot.overlapCenter
                    });
                }
            }
            return types::TaggedDeltaResResult<THandle, TComponentTag>{processedCollisions};
        }
    };
}