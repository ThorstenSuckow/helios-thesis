module;

#include <vector>

export module thesis.manager.LambdaManager;

import helios.gameplay.lifecycle.commands;
import helios.ecs.command;
import helios.ecs.entity;

import thesis.types;

export namespace thesis::manager {

    template <typename THandle>
    class LambdaManager {
    public:
        using LifetimeCommand = helios::gameplay::lifecycle::commands::LifetimeCommand<THandle>;
        using EntityManager = helios::ecs::entity::EntityManager<THandle>;

    private:
        std::vector<LifetimeCommand> pendingCommands_;

    public:
        LambdaManager() = default;

        bool init(helios::ecs::command::CommandHandlerRegistry& commandHandlerRegistry) noexcept {
            commandHandlerRegistry.handleCommands<LifetimeCommand>(*this);
            return true;
        }

        void reset() noexcept {
            pendingCommands_.clear();
        }

        bool submit(LifetimeCommand&& command) noexcept {
            pendingCommands_.push_back(std::move(command));
            return true;
        }

        /**
         * @note Not thread-safe!
         */
        types::LambdaManagerResult execute(EntityManager& entityManager) noexcept {
            bool result = true;

            auto expiredCount = pendingCommands_.size();

            for (const auto& command : pendingCommands_) {
                result = entityManager.destroy(command.handle) && result;
            }

            if (expiredCount > 0) {
                entityManager.finalizeMutations();
            }

            pendingCommands_.clear();
            return types::LambdaManagerResult{expiredCount};
        }
    };

}





