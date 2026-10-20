# Trace notes

Review status: pending human review.

Copies of hello are independent; assigning world to the copy gives helloworld when concatenated. Empty length is zero, and three NUL bytes retain length three. One hundred appended five-byte words grow 20000 bytes to 20500, preserved by BYVAL and RETURN. C++17 formatting gives 42 and 1.250000. Parsing 12.5 compares equal to 12.5. Trailing junk and negative counts emit the explicit library reasons at user lines 27 and 32, then are caught. Discarded numeric and Echo results must release their owned strings at statement end. The explicitly deleted heap object prints heap. This fixture covers compiler/library extensions, including numeric text formatting.

LEFT keeps ABC, returns zero bytes for an empty source or nonpositive count, and clamps 20 to all eight letters. Taking four bytes from three NULs followed by hello retains all four bytes.

RIGHT keeps FGH, returns zero bytes for an empty source or nonpositive count, and clamps 20 to all eight letters. Taking four bytes from hello followed by three NULs retains the final o and all three NULs.

The added checks use separate procedures to keep generated temporary-object cleanup small enough for the ordinary runner timeout.

MID positions 2..4 give BCD. The minimum INTEGER start clamps to 1 and gives AB; a maximum count from position 7 gives GH. Empty input, a maximum start, and nonpositive counts give length zero. Four bytes starting at the second of three NULs retain two NULs and AB.

LCASE converts AbC@19 to abc@19 without modifying the source. Its CHAR overload returns CHAR w for W or w and preserves @, as in guide section 5.5. Empty length stays zero; two leading NULs survive conversion and letters after them still become abc.

UCASE converts AbC@19 to ABC@19 without modifying the source. Its CHAR overload returns CHAR H for h or H and preserves @, as in guide section 5.5. Empty length stays zero; two leading NULs survive conversion and letters after them still become ABC.

ASC returns 65 for CHAR A and the first byte of ABCDEFGH, zero for empty strings or NUL, and 255 for a byte with all eight bits set. These byte-code functions are compiler/library extensions.
