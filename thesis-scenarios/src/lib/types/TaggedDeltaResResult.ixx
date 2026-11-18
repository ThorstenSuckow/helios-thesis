module;

#include <cstddef>

export module thesis.types.TaggedDeltaResResult;

export namespace thesis::types {


    template<typename THandle, typename TComponentTag>
    struct TaggedDeltaResResult {
        std::size_t processedCollisions;
    };


}
