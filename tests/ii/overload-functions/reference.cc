#include <cstdint>
#include <iostream>
#include <string>

int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int row = 0; row < count; ++row) {
        std::int64_t a, b, size, lower;
        std::string text;
        if (!(std::cin >> a >> b >> size >> lower >> text)) return 1;
        // Closed forms are independent of the pseudocode's loops and dispatch.
        const auto sum = size * a + b * size * (size - 1) / 2;
        std::cout << 3 * a + 1 << ':' << (a < 0 ? "TRUE" : "FALSE") << ':'
                  << text.size() + 100 << ':' << 17 * a + b << ':' << sum << ':'
                  << (sum > 0 ? "TRUE" : "FALSE") << ':' << 8 * a + b << ':'
                  << 9 * a + 4 << ':' << a + b * size << ':' << a << '\n';
    }
}
