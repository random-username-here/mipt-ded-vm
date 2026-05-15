# (idea) Linkable & debuggable format

This thing needs it.

## Header

 - Jump to non-linkable stub: `21 .. .. .. .. 30`
 - Signature: `"ILDF"`
 - Section count: `u16`
 - Section infos:
    - Name: `char64`
    - Flags: `u16` (no usage for now)
    - Offset: `u16`
    - Size in file: `u16`
    - Size in memory: `u16` (no usage for now, later for .bss)
 - non-linkable stub, like Microsoft PE's

## Section `.text`

Location for text & data. Execution starts here. Can be placed anywhere
(unlike fixed `0x1000` location in plain format), so no non-rjmp.

## Section `.sym`

Symbols.

 - Symbol count: `u16`
 - Symbol table:
     - Name offset: `u16`
     - Length: `u16`
     - Offset in `.text`: `u16`
     - Flags: `u16`
        - `ILDF_SYM_HERE`
        - `ILDF_SYM_FUNC`
        - `ILDF_SYM_BLOB`
        - `ILDF_SYM_STRING`
 - Zero-terminated names

## Section `.reloc`

Relocation data. For linking & dynamic loading.

 - Relocation count: `u16`
 - Relocations:
    - Offset: `u16`
    - Symbol index for which address is added: `u16`
    - Symbol index for which address is subtracted: `u16`
    - Scale factor: `u16`

This will require new IASM assembler...
