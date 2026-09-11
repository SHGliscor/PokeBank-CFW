# PokeBank-CFW

Native Nintendo 3DS homebrew Pokemon storage.

## v0.3-alpha

The first real game adapter is now included for Pokemon Omega Ruby / Alpha Sapphire.

Current features:
- Native CIA and 3DSX builds
- Persistent Bank on SD at sdmc:/3ds/PokeBank-CFW/bank.dat
- 100 Bank boxes x 30 slots = 3,000 slots
- Detects Omega Ruby / Alpha Sapphire from cartridge or installed SD title
- Opens the ORAS /main save read-only
- Reads all 31 ORAS PC boxes
- Decrypts boxed PK6 enough to identify species, PID, nature ID, ability ID, shiny state, and checksum validity
- Copies a selected native 232-byte PK6 into a selected Bank slot
- Keeps the original ORAS Pokemon untouched
- Refuses invalid PK6 checksums
- Requires a second X press before overwriting an occupied Bank slot

Important: v0.3-alpha does not write to the Pokemon game save. Only PokeBank-CFW bank.dat is modified.

## Controls

- D-Pad: move selected slot in the focused box
- L/R: change focused box
- Y: switch focus between Bank and ORAS
- A: inspect selected slot
- X: copy selected ORAS Pokemon into selected Bank slot
- B: refresh ORAS box or re-check Bank
- SELECT: rescan for ORAS
- START: exit

## Next milestones

1. Hardware-verify cartridge and SD ORAS detection
2. Verify all 31 ORAS boxes and PK6 decoding
3. Add automatic save backup support
4. Add safe ORAS withdrawal/write support with save resigning
5. Add Pokemon X/Y
6. Add Gen 7
7. Add VC Gen 1/2
8. Add DS Gen 4/5
9. Add Gen 3
10. Add official cross-generation transfer rules
