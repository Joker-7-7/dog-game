#include "players.h"
#include <iostream>
#include <stdexcept>

namespace app {

Player& Players::AddPlayer(model::Dog* dog, model::GameSession* session) {
    auto player = std::make_shared<Player>();
    player->id_ = Player::Id{static_cast<uint64_t>(players_.size())};
    player->dog_ = dog;
    player->session_ = session;
    const Token token = player_tokens_.GenerateToken();
    const auto it = players_.emplace(token, std::move(player)).first;
    return *it->second;
}

Player& Players::RestorePlayer(model::Dog* dog, model::GameSession* session,
                               Player::Id id, const Token& token) {
    auto player = std::make_shared<Player>();
    player->id_ = id;
    player->dog_ = dog;
    player->session_ = session;
    const auto [it, inserted] = players_.emplace(token, std::move(player));
    if (!inserted) {
        throw std::runtime_error("Duplicate player token in state file");
    }
    return *it->second;
}

std::shared_ptr<Player> Players::FindByDogIdAndMapId(model::Dog* dog, const model::Map::Id& map_id) const {
    for (const auto& [token, p] : players_) {
        if (p->dog_ == dog && p->session_ && p->session_->GetMap()
            && p->session_->GetMap()->GetId() == map_id) {
            return p;
        }
    }
    return nullptr;
}

std::shared_ptr<Player> Players::FindByToken(const Token& token) const {
    std::cout << *token << std::endl;
    if (auto it = players_.find(token); it != players_.end()) {
        return it->second;
    }
    return nullptr;
}

Token Players::FindToken(const Player* player) const {
    for (const auto& [token, p] : players_) {
        if (p.get() == player) {
            return token;
        }
    }
    return Token{std::string{}};
}
const Players::TokenToPlayer& Players::GetPlayers() const
{
    return players_;
}

}  // namespace app
