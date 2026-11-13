#include "static_request_handler.h"
#include "utils.h"

#include <fstream>

namespace fs = std::filesystem;

using namespace http_handler;
using namespace utils;

std::optional<std::string_view> StaticRequestHandler::GetContentType(fs::path fileExtension) 
{
    if(fileExtension == ".html" || fileExtension == ".htm")
    {
        return ContentType::TEXT_HTML;
    }
    else if(fileExtension == ".txt")
    {
        return ContentType::TEXT_PLAIN;
    }
    else if(fileExtension == ".js")
    {
        return ContentType::TEXT_JS;
    }
    else if(fileExtension == ".json")
    {
        return ContentType::APP_JSON;
    }
    else if(fileExtension == ".xml")
    {
        return ContentType::APP_XML;
    }
    else if(fileExtension == ".png")
    {
        return ContentType::IMAGE_PNG;
    }
    else if(fileExtension == ".jpeg" || fileExtension == ".jpg" || fileExtension == ".jpe")
    {
        return ContentType::IMAGE_JPEG;
    }
    else if(fileExtension == ".gif")
    {
        return ContentType::IMAGE_GIF;
    }
    else if(fileExtension == ".bmp")
    {
        return ContentType::IMAGE_BMP;
    }
    else if(fileExtension == ".ico")
    {
        return ContentType::IMAGE_MSICON;
    }
    else if(fileExtension == ".tiff" || fileExtension == ".tif")
    {
        return ContentType::IMAGE_TIFF;
    }
    else if(fileExtension == ".svg" || fileExtension == ".svgz")
    {
        return ContentType::IMAGE_SVG;
    }
    else if(fileExtension == ".mpeg")
    {
        return ContentType::AUIDO_MPEG;
    }
    return ContentType::TEXT_HTML;
}

StringResponse StaticRequestHandler::operator()(const StringRequest& req) 
{
    fs::path path{std::string(req.target())};
    auto respond = [&req](
        http::status status,
        std::string_view body,
        std::string_view content_type)
    {
        return MakeResponse(
            status, body, req.version(), req.keep_alive(), content_type);
    };

    const fs::path canonical_root = fs::weakly_canonical(root_);
    path = (path == "/") ? fs::path{"index.html"} : path.relative_path();

    const fs::path file_path =
        fs::weakly_canonical(canonical_root / path);

    if (IsSubPath(file_path, canonical_root) == false) {
        return respond(
            http::status::bad_request, BadRequest, ContentType::TEXT_PLAIN);
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return respond(
            http::status::not_found, NotFound, ContentType::TEXT_PLAIN);
    }

    const std::string content{
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}
    };

    const auto content_type = GetContentType(file_path.extension());
    if (!content_type) {
        return respond(
            http::status::not_found, NotFound, ContentType::TEXT_PLAIN);
    }

    return respond(http::status::ok, content, *content_type);
}