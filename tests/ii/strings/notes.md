# strings

CIE pp. 16-17 for LENGTH/MID/RIGHT/concatenation; full-STRING LCASE/UCASE is a project extension. Inputs are ASCII under the default C locale. Empty input is encoded with a flag because project INPUT reads tokens. Substrings remain in range; zero width is a documented project boundary.

Each record produces exactly one output line. Random records are preceded by fixed boundary records; every arithmetic intermediate is bounded.
