# strings

CIE pp. 16-17 for LENGTH/MID/RIGHT/concatenation; full-STRING LCASE/UCASE is a project extension. Inputs are ASCII under the default C locale. The text is the last field of each record because INPUT into a STRING reads the rest of the line; empty text is still encoded with a flag so that every record has a visible text field. Substrings remain in range; zero width is a documented project boundary.

Each record produces exactly one output line. Random records are preceded by fixed boundary records; every arithmetic intermediate is bounded.
