# Trace notes

Review status: pending human review.

Mixed has an ordinary header but enables __LIB inside its body. Its first division uses the caller's line 2. After UNDEF, its own division uses regions.cps line 10 and a nested library call uses line 15. A disabled DEFINE does not enable __LIB, so the next division uses line 23. The library template retains its definition's mode during deferred emission and reports its caller's line 4. DeleteOnExit destroys its local object on return; the automatic library destructor retains the ordinary caller's line 9. Every error is caught and prints the next integer from 1 through 6. These are compiler extensions.
