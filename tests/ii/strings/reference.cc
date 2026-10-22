#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int i = 0; i < count; ++i) {
        int empty, start, width;
        std::string text;
        if (!(std::cin >> empty >> start >> width)) return 1;
        // The text is the rest of the line, as for INPUT into a STRING.
        if (!std::getline(std::cin >> std::ws, text)) return 1;
        if (!text.empty() && text.back() == '\r') text.pop_back();
        if (empty) text.clear();
        auto lower = text, upper = text;
        for (auto &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        auto mid = text.substr(static_cast<std::size_t>(start - 1), static_cast<std::size_t>(width));
        auto right = text.substr(text.size() - static_cast<std::size_t>(width));
        std::cout << text.size() << ':' << mid << ':' << right << ":TRUE:" << lower << ':' << upper << '\n';
    }
}
