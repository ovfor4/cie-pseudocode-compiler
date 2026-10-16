# class-default-public

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: CIE p. 28: methods and properties are public unless otherwise stated. This conflicts with the current explicit-access requirement.

Trace: Value has default public access. Assigning and reading it must yield 7. A compile rejection is a failure, not an accepted alternative.
