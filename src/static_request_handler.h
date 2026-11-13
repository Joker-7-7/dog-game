#pragma once 

#include "model.h"
#include "http_server.h"

#include <filesystem>
#include <optional>

namespace http_handler 
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>; 
    
    class StaticRequestHandler {
        public:
            explicit StaticRequestHandler(model::Game& game, std::filesystem::path root):
                game_{game},
                root_{root}
            {
            }
        
        StringResponse operator()(const StringRequest& req);
        std::optional<std::string_view> GetContentType(std::filesystem::path fileExtension);

        private:
            model::Game& game_;
            std::filesystem::path root_;
    };
}