# caught-errors

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: Project TRY and runtime-check contract; diagnostics precede catch execution on stdout.

Trace: Each failed access transfers to its CATCH, so no attempted value is printed. Source lines 7,12,17,22 identify the operations. Bare THROW has no own diagnostic; execution reaches alive.
