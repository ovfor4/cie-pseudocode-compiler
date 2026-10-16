#include <cstdint>
#include <iostream>
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int i = 0; i < count; ++i) {
        std::int64_t a;
        int b;
        if (!(std::cin >> a >> b)) return 1;
        auto l = static_cast<long>(a);
        auto c = static_cast<char>(b);
        auto u = static_cast<unsigned char>(b);
        auto w = static_cast<unsigned int>(a);
        std::cout << l << ':' << static_cast<std::int64_t>(c) << ':'
                  << u + static_cast<unsigned char>(1) << ':'
                  << w + static_cast<unsigned int>(1) << '\n';
    }
}
