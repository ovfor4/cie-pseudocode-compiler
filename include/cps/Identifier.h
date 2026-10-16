#pragma once
#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace cps {

inline unsigned char foldIdentifier(unsigned char C) {
    return C >= 'A' && C <= 'Z' ? C + ('a' - 'A') : C;
}

struct IdentifierLess {
    bool operator()(const std::string &A, const std::string &B) const {
        return std::lexicographical_compare(A.begin(), A.end(), B.begin(), B.end(),
            [](unsigned char L, unsigned char R) { return foldIdentifier(L) < foldIdentifier(R); });
    }
};

inline bool sameIdentifier(const std::string &A, const std::string &B) {
    return !IdentifierLess{}(A, B) && !IdentifierLess{}(B, A);
}

template<class T> using IdentifierMap = std::map<std::string, T, IdentifierLess>;
using IdentifierSet = std::set<std::string, IdentifierLess>;

} // namespace cps
