#include <string>
#include <stdexcept>
extern "C" int string_length() {
    std::string text(4096, 'x');
    text += std::to_string(123);
    return static_cast<int>(text.size());
}
extern "C" void string_throw() {
    try { throw std::invalid_argument("string failure"); }
    catch (const std::exception &e) {
        if (std::string(e.what()) == "string failure") throw;
    }
}
