# loops

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE pp. 20-21.

Trace: FOR contributions are 0, 2, and 3+1-1=3. WHILE contributes 0+1+2=3. REPEAT contributes 10 once, then counts to 3. Total=18.

The project requires DO after WHILE; this fixture uses that spelling to exercise runtime semantics. The separate standard-while fixture tests the CIE spelling.
