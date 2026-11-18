module;


export module thesis.commands:TaggedCollisionCommand;

import helios.math;

export namespace thesis::commands {

    enum class CollisionCommandType {
        Impact
    };

    template<typename THandle, typename TComponentTag>
    struct TaggedCollisionCommand {

        THandle leftHandle;
        THandle rightHandle;

        CollisionCommandType type;

        helios::math::vec3f location;

    };

}