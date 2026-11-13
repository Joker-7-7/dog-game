#pragma once

#include <map>
#include <memory>

#include "app.h"
#include "player_tokens.h"

namespace app {

class Players {
public:
    using TokenToPlayer = std::map<Token, std::shared_ptr<Player>>;
    
    Player& AddPlayer(model::Dog* dog, model::GameSession* session);

    // Adds a player with the given id and token when restoring state.
    // Throws if the token is already taken.
    Player& RestorePlayer(model::Dog* dog, model::GameSession* session,
                          Player::Id id, const Token& token);

    std::shared_ptr<Player> FindByDogIdAndMapId(model::Dog* dog, const model::Map::Id& map_id) const;
    std::shared_ptr<Player> FindByToken(const Token& token) const;

    // Returns an empty token if the player is not found
    Token FindToken(const Player* player) const;

    const TokenToPlayer& GetPlayers() const;
private:
    

    PlayerTokens player_tokens_;
    TokenToPlayer players_;
};

}  // namespace app
