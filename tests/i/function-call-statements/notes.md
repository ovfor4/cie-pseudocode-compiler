# Trace notes

Human review: pending.

This case exercises compiler extensions. Pointer casts preserve the addresses of INTEGER 17 and C int 23. Echo prints 7 even when its result is discarded. Discarded LENGTH calls print nothing. Two BYREF procedure calls increment 17 to 19. The argument division on line 33 throws before Echo runs; CATCH prints caught.
