# Trace notes

Review status: pending human review.

Each completed default/copy construction increments Live; only copy construction increments Copies. Pass destroys its independent modified copy. Make copies its local into the result before destroying the local. Forward transfers that result. Discarded results, temporary receivers and constructor-bound results are destroyed at statement end. A later argument throwing destroys the already-created argument before entering CATCH. At the final count, nine copies have completed and only A, B and C remain live; reverse destruction removes values 8, 1 and 1. Remaining is zero. This is the class lifetime extension.
