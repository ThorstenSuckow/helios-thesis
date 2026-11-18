module;

#include <functional>
#include <span>
#include <utility>
#include <vector>

export module thesis.manager.TaggedDeltaManager;

import thesis.types;
import thesis.commands;

import helios.ecs.command;
import helios.ecs.entity.EntityManager;
import helios.engine.runtime.gameloop.types;

export namespace thesis::manager {

    template <typename TEmitterHandle, typename TTargetHandle, typename TComponentTag>
    class TaggedDeltaManager {
    public:
        using CollisionCommand = thesis::commands::TaggedCollisionCommand<TEmitterHandle, TComponentTag>;
        using CollisionCommands = std::span<const CollisionCommand>;
        using ExecuteLambda = std::function<void(
            helios::ecs::entity::EntityManager<TTargetHandle>&,
            CollisionCommands,
            const helios::engine::runtime::gameloop::types::UpdateContext&
        )>;

    private:
        ExecuteLambda executeLambda_;
        std::vector<CollisionCommand> pendingCommands_;

    public:
        TaggedDeltaManager() = delete;

        explicit TaggedDeltaManager(ExecuteLambda executeLambda)
            : executeLambda_(std::move(executeLambda)),
              pendingCommands_{} {
        }

        bool init(helios::ecs::command::CommandHandlerRegistry& commandHandlerRegistry) noexcept {
            commandHandlerRegistry.handleCommands<CollisionCommand>(*this);
            return true;
        }

        void reset() noexcept {
            pendingCommands_.clear();
        }

        bool submit(CollisionCommand&& command) noexcept {
            pendingCommands_.push_back(std::move(command));
            return true;
        }

        types::DeltaManagerResult execute(
            helios::ecs::entity::EntityManager<TTargetHandle>& targetEntityManager,
            const helios::engine::runtime::gameloop::types::UpdateContext& updateContext
        ) noexcept {
            auto size = pendingCommands_.size();

            executeLambda_(targetEntityManager, pendingCommands_, updateContext);
            pendingCommands_.clear();

            return types::DeltaManagerResult{size};
        }
    };

}





