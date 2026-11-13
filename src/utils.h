#pragma once

#include "http_server.h"
#include "model.h"

#include <filesystem>

namespace utils 
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>;

    constexpr auto BadRequest =  R"({"code": "badRequest", "message": "Bad request"})";
    constexpr auto NotFound =  R"({"code": "notFound", "message": "Not found"})";
    constexpr auto MapNotFound = R"({"code": "mapNotFound", "message": "Map not found"})";
    constexpr auto InvalidArgument = R"({"code": "invalidArgument", "message": "Invalid argument"})";
    constexpr auto InvalidMethod = R"({"code": "invalidMethod", "message": "Invalid method"})";
    constexpr auto InvalidToken = R"({"code": "invalidToken", "message": "Authorization header is missing"})";
    constexpr auto UnknownToken = R"({"code": "unknownToken", "message": "Player token has not been found"})";

    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view TEXT_HTML = "text/html"sv;
        constexpr static std::string_view TEXT_CSS = "text/css"sv;
        constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
        constexpr static std::string_view TEXT_JS = "text/javascript"sv;
        constexpr static std::string_view APP_JSON = "application/json"sv;
        constexpr static std::string_view APP_XML = "application/xml"sv;
        constexpr static std::string_view IMAGE_PNG = "image/png"sv;
        constexpr static std::string_view IMAGE_JPEG = "image/jpeg"sv;
        constexpr static std::string_view IMAGE_GIF = "image/gif"sv;
        constexpr static std::string_view IMAGE_BMP = "image/bmp"sv;
        constexpr static std::string_view IMAGE_MSICON = "image/vnd.microsoft.icon"sv;
        constexpr static std::string_view IMAGE_TIFF = "image/tiff"sv;
        constexpr static std::string_view IMAGE_SVG = "image/svg+xml"sv;
        constexpr static std::string_view AUIDO_MPEG = "auido/mpeg"sv;
    };


    StringResponse MakeResponse(
        http::status status, 
        std::string_view body, 
        unsigned http_version,
        bool keep_alive,
        std::string_view content_type = ContentType::TEXT_HTML
    );

    bool IsSubPath(std::filesystem::path path, std::filesystem::path base);

    model::Position GetSpawnPoint(const model::Map& map, bool randomize);
}