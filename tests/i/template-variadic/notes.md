# Variadic function templates

Human review: pending.

TEMPLATE and parameter packs are compiler extensions. PrintAll outputs each head before recursively forwarding the remaining arguments. Its ordinary parameterless overload terminates the recursion and produces no output. The first call therefore prints INTEGER 1, REAL 2.500000, CHAR c and STRING str in order; the direct empty call prints nothing. Explicit INTEGER binds the first type of the next call, which prints 1 and 2.500000.

Forward explicitly binds the first pack entry to INTEGER, deduces its REAL tail and forwards both the type pack and value pack, producing 7 and 3.500000. Its empty instantiation forwards empty packs to the parameterless base case and produces no output.

Pick returns 30 for the exact ordinary INTEGER overload, 10 for the fixed template on REAL, and 20 for the variadic two-argument template. Explicit INTEGER selects the fixed template and returns 10. REAL formatting follows the project's six-decimal output convention.
