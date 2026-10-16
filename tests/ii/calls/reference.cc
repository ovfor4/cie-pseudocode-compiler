#include <cstdint>
#include <iostream>
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int i = 0; i < count; ++i) {
        std::int64_t n, x, y;
        if (!(std::cin >> n >> x >> y)) return 1;
        std::cout << x + y << ':' << x << ':' << x + 1 << ':' << y - 1
                  << ':' << n * (n + 1) / 2 << '\n';
    }
}
