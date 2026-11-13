#pragma once 

#include "model.h"
#include "http_server.h"
#include "players.h"

#include <filesystem>
#include <random>

namespace http_handler 
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>; 
    
    class ApiRequestHandler {
        public:
            explicit ApiRequestHandler(model::Game& game, app::Players& players, std::filesystem::path root, bool is_tick_mode, bool randomize_spawn_points):
                isTickMode{is_tick_mode},
                randomizeSpawnPoints{randomize_spawn_points},
                game_{game},
                root_{root},
                players_{players}
            {
            }

        StringResponse operator()(const StringRequest& req);

        private:
            StringResponse HandleJoin(const StringRequest& req);
            StringResponse HandleMaps(const StringRequest& req);
            StringResponse HandlePlayers(const StringRequest& req);
            StringResponse HandleState(const StringRequest& req);
            StringResponse HandleAction(const StringRequest& req);
            StringResponse HandleTick(const StringRequest& req);

            bool isTickMode = false;
            bool randomizeSpawnPoints = false;
            model::Game& game_;
            std::filesystem::path root_;
            app::Players& players_;
    };
}