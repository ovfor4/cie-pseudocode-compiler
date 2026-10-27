#include <cstddef>
#include <cstdio>
#include <cstdlib>

extern "C" int __cps_user_line;

namespace {
struct Allocation {
    void *Address;
    Allocation *Next;
};
Allocation *Head = nullptr;

[[noreturn]] void fail(const char *Reason) {
    std::fprintf(stderr, "[Fatal] line %d: %s\n", __cps_user_line, Reason);
    throw 0;
}

// Return the link owning the node so head and interior removals are identical.
Allocation **find(void *Address) {
    for (Allocation **Link = &Head; *Link; Link = &(*Link)->Next)
        if ((*Link)->Address == Address) return Link;
    fail("Invalid memory address (not an active allocation)");
}
}

extern "C" {
void *_memory_impl_malloc(unsigned long long Bytes) {
    std::size_t Size = static_cast<std::size_t>(Bytes);
    if (!Size) return nullptr;
    auto *Node = static_cast<Allocation *>(std::malloc(sizeof(Allocation)));
    if (!Node) fail("Allocation tracking failed");
    void *Address = std::malloc(Size);
    if (!Address) {
        std::free(Node);
        fail("Memory allocation failed");
    }
    Node->Address = Address;
    Node->Next = Head;
    Head = Node;
    return Address;
}

void _memory_impl_validate(void *Address) {
    find(Address);
}

void *_memory_impl_free(void *Address) {
    if (Address) {
        Allocation **Link = find(Address);
        Allocation *Node = *Link;
        *Link = Node->Next;
        std::free(Address);
        std::free(Node);
    }
    return Address;
}

void *_memory_impl_realloc(void *Address, unsigned long long Bytes) {
    std::size_t Size = static_cast<std::size_t>(Bytes);
    if (!Address) return _memory_impl_malloc(Bytes);
    Allocation *Node = *find(Address);
    if (!Size) {
        _memory_impl_free(Address);
        return nullptr;
    }
    void *Resized = std::realloc(Address, Size);
    // Failure preserves both the original allocation and its tracking record.
    if (!Resized) fail("Memory allocation failed");
    Node->Address = Resized;
    return Resized;
}
}
