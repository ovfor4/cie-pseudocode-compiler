# template-explicit-args

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: TEMPLATE is a project extension. Max explicitly selects REAL and INTEGER, producing 2.000000 and 3. Second binds its first type explicitly and deduces STRING for its second, returning tail. Zero's otherwise undeducible return type is explicitly REAL, producing 0.000000. CALL and bare Print statements produce 2.500000 and 9. Pick uses the ordinary exact overload without explicit arguments (200) and the template with them (100). Echo preserves the STRING chain through a result method, and discarded STRING results exercise statement cleanup without output.
