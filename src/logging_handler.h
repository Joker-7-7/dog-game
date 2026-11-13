
#pragma once

#include <chrono>

#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include "request_handler.h"

namespace http_handler 
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace logging = boost::log;

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>; 
    

    template<class SomeRequestHandler>
    class LoggingRequestHandler 
    {
    public:
        LoggingRequestHandler(SomeRequestHandler& handler) : decorated_(handler)
        {
            InitLogging();
        }

        void InitLogging()
        {
            logging::add_common_attributes();

            logging::add_console_log(
                std::cout,
                boost::log::keywords::format = &LoggingRequestHandler::JsonFormatter,
                boost::log::keywords::auto_flush = true
            );
        }

    static void JsonFormatter(
        logging::record_view const& rec,
        logging::formatting_ostream& strm
    )
        {
            boost::json::object json;

            auto ts = *rec[timestamp];
            json["timestamp"] = boost::posix_time::to_iso_extended_string(ts);
            

            if (auto d = rec[additional_data]) {
                json["data"] = *d;
            }

            if (auto message = rec[logging::expressions::smessage]) {
                json["message"] = message.get();
            }

            strm << boost::json::serialize(json);
        }

        template <typename Body, typename Allocator, typename Send>
        void operator()(
            const http_server::tcp::endpoint& endpoint,
            http::request<Body, http::basic_fields<Allocator>>&& req,
            Send&& send)
        {
            LogRequest(req, endpoint);

            const auto start = std::chrono::steady_clock::now();

            decorated_(std::move(req),
                [endpoint, start, send = std::forward<Send>(send)](StringResponse&& response) mutable {
                    const auto response_time =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start
                        ).count();

                    LogResponse(response, response_time, endpoint);

                    send(std::move(response));
                });
        }

    private:
        template <typename Body, typename Allocator>
        static void LogRequest(
            const http::request<Body, http::basic_fields<Allocator>>& r,
            const http_server::tcp::endpoint& endpoint)
        {
            boost::json::object data{
                {"ip", endpoint.address().to_string()},
                {"URI", std::string(r.target())},
                {"method", std::string(r.method_string())}
            };

            BOOST_LOG_TRIVIAL(info)
                << boost::log::add_value(
                    additional_data,
                    boost::json::value(std::move(data)))
                << "request received";
        }

        static void LogResponse(
            const StringResponse& r,
            std::int64_t response_time,
            const http_server::tcp::endpoint& endpoint)
        {
            boost::json::object data{
                {"ip", endpoint.address().to_string()},
                {"response_time", response_time},
                {"code", r.result_int()}
            };

            const auto it = r.find(http::field::content_type);

            if (it != r.end()) {
                data["content_type"] = std::string(it->value());
            } else {
                data["content_type"] = nullptr;
            }

            BOOST_LOG_TRIVIAL(info)
                << boost::log::add_value(
                    additional_data,
                    boost::json::value(std::move(data)))
                << "response sent";
        }

        SomeRequestHandler& decorated_;
    };
}
