# Destructor regression

Human review: pending. Expected output was independently traced by AI.

A destructor throwing on normal return clears its alive flag and the unwind pad destroys the remaining Trace once. RETURN in CATCH preserves catch cleanup; RETURN inside TRY preserves its local handler. Function return values are evaluated before destruction.

A destructor throwing during exception cleanup propagates to the caller instead of terminating.
