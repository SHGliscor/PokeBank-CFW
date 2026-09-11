# PokeBank-CFW

Native Nintendo 3DS homebrew Pokemon storage.

## v0.4-alpha

Hardware status:
- ORAS title detection: PASS
- ORAS 31-box reading: PASS
- PK6 decryption/checksum/shiny decoding: PASS
- ORAS -> Bank copy: PASS
- Bank -> ORAS empty-slot copy: PASS
- ORAS save checksum resigning + commit: PASS
- In-game save/reboot persistence after withdrawal: PASS
- Bank -> ORAS occupied-slot overwrite: PASS
- ORAS storage adapter: COMPLETE / HARDWARE VERIFIED
- Bank persistence after reboot: PASS
- Original ORAS Pokemon remains untouched after deposit: PASS

New in v0.4-alpha:
- automatic rotating Bank backups before every Bank write
- automatic rotating ORAS /main backup before every game-save write
- Bank -> ORAS copy mode (hardware-verified for empty destination slots)
- ORAS box checksum resigning (storage block)
- save archive commit
- raw PK6 and box-checksum read-back verification
- occupied destination requires a second X press

Safety: Bank -> ORAS currently COPIES the Pokemon and keeps the Bank copy. It does not delete the Bank source.

Backups:
- sdmc:/3ds/PokeBank-CFW/backups/bank-latest.bak
- sdmc:/3ds/PokeBank-CFW/backups/bank-previous.bak
- sdmc:/3ds/PokeBank-CFW/backups/OR-main-latest.bak / previous
- sdmc:/3ds/PokeBank-CFW/backups/AS-main-latest.bak / previous

Controls:
- Y switches focus between GAME and BANK
- X while GAME focused: copy GAME -> BANK
- X while BANK focused: copy BANK -> selected GAME slot
- occupied destination: press X twice
- L/R changes boxes on focused side
- D-Pad changes selected slot
- A inspects
- B refreshes
- SELECT rescans ORAS
- START exits

ORAS storage support is fully hardware-verified, including empty-slot withdrawal, occupied-slot overwrite, backup creation, save commit, checksum resigning, in-game save, and reboot persistence.