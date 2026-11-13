#pragma once

#include <string>

#include "model.h"

namespace app {

namespace detail {
    struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class Player {
public:
    using Id = util::Tagged<uint64_t, Player>;

    Id id_{0};
    model::GameSession* session_ = nullptr;
    model::Dog* dog_ = nullptr;
};

}  // namespace app
