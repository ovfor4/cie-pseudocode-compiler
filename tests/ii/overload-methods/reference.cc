#include <cstdint>
#include <iostream>
#include <string>

int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int row = 0; row < count; ++row) {
        std::int64_t a, b, size;
        std::string text;
        if (!(std::cin >> a >> b >> size >> text)) return 1;
        // Model the resulting values directly, without reproducing class dispatch.
        const auto number = 2 * a + 3;
        const auto length = text.size();
        std::cout << 5 << ':' << number << ':' << length + 20 << ':' << a - b
                  << ':' << 5 << ':' << 5 * size << ':' << number + 3 * b << ':'
                  << (number > b / 8.0 ? "TRUE" : "FALSE") << ':' << text << "!:"
                  << a - b << ':';
        std::cout << length << ':' << a + 3 * b << ':' << length + 20 << '\n';
    }
}
