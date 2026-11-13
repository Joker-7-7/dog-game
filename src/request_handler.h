#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <filesystem>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/log/expressions/keyword.hpp>

#include "model.h"
#include "api_request_handler.h"
#include "static_request_handler.h"
#include "players.h"
#include "utils.h"
#include "ticker.h"

namespace http_handler
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;

    BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)
    BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>;

    class RequestHandler {
    public:
        explicit RequestHandler(
            model::Game& game,
            app::Players& players,
            std::filesystem::path root,
            net::strand<net::io_context::executor_type> api_strand,
            int64_t tick_period,
            bool randomize_spawn_points) :
                game_{game},
                root_{root},
                players_{players},
                tick_period_{tick_period},
                apiHandler_{game, players, root, tick_period > 0, randomize_spawn_points},
                staticHandler_{game, root},
                api_strand_{std::move(api_strand)}
        {
            if (tick_period_ > 0) {
                ticker_ = std::make_shared<Ticker>(
                    api_strand_,
                    std::chrono::milliseconds{tick_period_},
                    [this](std::chrono::milliseconds delta) {
                        game_.Tick(delta.count());
                    });
                ticker_->Start();
            }
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            const std::filesystem::path path{std::string(req.target())};
            const std::filesystem::path api_root = "/api";

            if (path == api_root || utils::IsSubPath(path, api_root)) {
                net::dispatch(api_strand_,
                    [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                        send(apiHandler_(std::move(req)));
                    });
                return;
            }

            send(staticHandler_(std::move(req)));
        }

    private:
        model::Game& game_;
        std::filesystem::path root_;
        app::Players& players_;
        int64_t tick_period_ = 0;
        ApiRequestHandler apiHandler_;
        StaticRequestHandler staticHandler_;
        net::strand<net::io_context::executor_type> api_strand_;
        std::shared_ptr<Ticker> ticker_;
    };
}
