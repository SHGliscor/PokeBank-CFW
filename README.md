# PokeBank-CFW

Native Nintendo 3DS Pokémon storage targeting Generations 1–7.

## Current milestone — v0.2 alpha

PokeBank-CFW now has the on-console storage foundation rather than only a smoke test:

- Native Nintendo 3DS application
- `.3dsx` and installable `.cia` build targets
- GitHub Actions cloud compilation using devkitARM/libctru
- Persistent Bank at `sdmc:/3ds/PokeBank-CFW/bank.dat`
- Versioned Bank header with format validation
- 100 Bank boxes × 30 slots = 3,000 Pokémon slots
- Fixed 512-byte records so native Gen 1–7 Pokémon payloads can be preserved without converting everything to Gen 7
- D-pad slot navigation
- L/R box navigation
- Slot inspection
- SD creation/read validation
- Home Menu icon/banner assets

## Safety state

The current alpha **does not modify Pokémon game saves yet**. It only creates and reads PokeBank-CFW's own SD-card Bank file. Game writes will not be enabled until the relevant save adapter has backup, validation, checksum and read-back verification.

## Planned game adapters

1. Gen 6 — Pokémon X/Y and Omega Ruby/Alpha Sapphire
2. Gen 7 — Sun/Moon and Ultra Sun/Ultra Moon
3. Gen 1/2 — official 3DS Virtual Console titles
4. Gen 4/5 — DS cartridges and compatible SD save sources
5. Gen 3 — compatible GBA/VC/SD save sources

ORAS is the first deposit/withdraw target.

## Bank design

A deposited Pokémon remains in its native generation format. For example, a Crystal Pokémon is stored as its Gen 2 payload and an Alpha Sapphire Pokémon as its Gen 6 payload. Cross-generation conversion will be a separate transfer operation rather than something that happens automatically on deposit.

## Building

No local compiler is required for normal testing. Push to `main` or run **Build PokeBank-CFW** from GitHub Actions. Successful runs upload a build artifact containing the `.cia`, `.3dsx`, `.smdh`, checksums and testing instructions.
