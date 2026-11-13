#include "player_tokens.h"

#include <iomanip>
#include <sstream>

namespace app {

Token PlayerTokens::GenerateToken() {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << generator1_()
        << std::hex << std::setw(16) << std::setfill('0') << generator2_();
    return Token{out.str()};
}

}  // namespace app
