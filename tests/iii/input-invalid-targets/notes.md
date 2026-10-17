# Invalid INPUT targets

Constants, private fields and whole arrays are not writable scalar locations. Whole records, enums, pseudocode pointers and C pointers remain unsupported INPUT value types. These diagnostics preserve the existing restrictions while accepting scalar array elements and public fields.
