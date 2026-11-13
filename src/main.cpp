#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

#include "json_loader.h"
#include "logging_handler.h"
#include "http_server.h"
#include "players.h"
#include "state_serialization.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

struct Args {
    int64_t tick_period = 0;  // ms
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    std::string state_file;
    int64_t save_state_period = 0;  // ms
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    Args args;
    desc.add_options()
        ("help,h", "Show help")
        ("tick-period,t", po::value(&args.tick_period)->value_name("ms"s), "Tick period in milliseconds")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "Path to game config JSON")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "Path to static files")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "Randomize spawn points")
        ("state-file", po::value(&args.state_file)->value_name("file"s), "Path to serialize server state to")
        ("save-state-period", po::value(&args.save_state_period)->value_name("ms"s),
         "Period of automatic state saving in game time milliseconds");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file has not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Www root has not been specified"s);
    }
    if (args.tick_period < 0) {
        throw std::runtime_error("Tick period must be non-negative"s);
    }
    if (args.save_state_period < 0) {
        throw std::runtime_error("Save state period must be non-negative"s);
    }

    return args;
}

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);

    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            model::Game game = json_loader::LoadGame(args->config_file);

            const unsigned num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(num_threads);

            const auto api_strand = net::make_strand(ioc);

            app::Players players;

            const bool has_state_file = !args->state_file.empty();
            if (has_state_file && std::filesystem::exists(args->state_file)) {
                serialization::LoadGameState(game, players, args->state_file);
            }

            if (has_state_file) {
                game.SetStateSavePeriod(args->save_state_period);
                game.SetStateSaveCallback([&game, &players, state_file = args->state_file] {
                    try {
                        serialization::SaveGameState(game, players, state_file);
                    } catch (const std::exception& ex) {
                        BOOST_LOG_TRIVIAL(error)
                            << "Failed to save game state: " << ex.what();
                    }
                });
            }

            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc](const sys::error_code& ec,
                                      [[maybe_unused]] int signal_number) {
                if (!ec) {
                    ioc.stop();
                }
            });

            http_handler::RequestHandler handler{game, players, args->www_root, api_strand, args->tick_period, args->randomize_spawn_points};
            http_handler::LoggingRequestHandler<http_handler::RequestHandler> loggerHandler(handler);

            const auto address = net::ip::make_address("0.0.0.0");
            constexpr net::ip::port_type port = 8080;
            http_server::ServeHttp(ioc, {address, port}, [&loggerHandler](const auto& endpoint, auto&& req, auto&& send) {
                loggerHandler(
                    std::forward<decltype(endpoint)>(endpoint),
                    std::forward<decltype(req)>(req),
                    std::forward<decltype(send)>(send));
            });

            BOOST_LOG_TRIVIAL(info)
                << boost::log::add_value(
                    http_handler::additional_data,
                    boost::json::value{
                        {"port", 8080},
                        {"address", "0.0.0.0"}
                    })
                << "Server has started";

            RunWorkers(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });

            if (has_state_file) {
                try {
                    serialization::SaveGameState(game, players, args->state_file);
                } catch (const std::exception& ex) {
                    BOOST_LOG_TRIVIAL(error)
                        << "Failed to save game state: " << ex.what();
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
