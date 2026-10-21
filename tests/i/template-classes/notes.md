# Class templates

Human review: pending.

TEMPLATE is a compiler extension. Holder forces Box<INTEGER> to instantiate during the type pre-pass. Make<Box<BOOLEAN>>() first creates Box<BOOLEAN> through an explicit function argument and returns its default FALSE value. Holder's integer field defaults to zero. Box's explicit constructor then stores 7. Pair preserves its INTEGER and REAL fields, giving 3 and 2.500000. Stack3 pushes 10, 20 and 30 into three fixed scalar slots and pops them in reverse order.

Keyword NEW constructs a fresh Box containing 5; DELETE releases that heap object. The nested Box copies the inner integer value 9. Box<STRING> assignment copies its owning STRING field independently: changing Text to beta leaves Copy equal to alpha, and both values remain valid through cleanup. Group expands three method parameters and forwards them to PrintAll, yielding 1, 2.500000 and c. ShowGroup() instantiates an internally empty Group<> and its parameterless Show, producing no output.
