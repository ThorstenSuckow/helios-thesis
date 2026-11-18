module;

#include <span>
export module thesis.types.TaggedDeltaDetResult;

import helios.physics.collision.types;


export namespace thesis::types{


    template<typename THandle, typename TComponentTag>
    struct TaggedDeltaDetResult {

        using CollisionPair = helios::physics::collision::types::CollisionPair<THandle>;
        using CollisionDetectionResult = helios::physics::collision::types::CollisionDetectionResult<THandle>;

        CollisionDetectionResult result_;

    public:


        TaggedDeltaDetResult(
            CollisionDetectionResult&& result
        ) : result_(std::move(result)) {}


        [[nodiscard]] std::span<const CollisionPair> collisionPairs() const {
            return result_.collisionPairs();
        }

    };


}