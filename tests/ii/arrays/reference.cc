#include <cstdint>
#include <iostream>
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int record = 0; record < count; ++record) {
        std::int64_t lower, size, step, base;
        if (!(std::cin >> lower >> size >> step >> base) || size < 1) return 1;
        // Closed form is independent of pseudocode's indexed loop.
        auto sum = size * base + step * size * (size - 1) / 2;
        std::cout << sum << ':' << base << ':' << base + (size - 1) * step << '\n';
    }
}
