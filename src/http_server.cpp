#include "http_server.h"

#include <iostream>

using namespace http_server;

void http_server::ReportError(
    boost::beast::error_code const& ec,
    std::string_view operation)
{
    std::cerr << operation << ": " << ec.message() << '\n';
}

