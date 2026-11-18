module;

#include <cstddef>

export module thesis.types.UniDeltaManagerResult;

export namespace thesis::types {


    struct UniDeltaManagerResult {
        std::size_t collisionCountGo;
        std::size_t collisionCountPrt;
    };


}
