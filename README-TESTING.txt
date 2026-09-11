POKEBANK-CFW v0.3-alpha - ORAS READ-ONLY ADAPTER TEST
=====================================================

This build DOES NOT write to Omega Ruby / Alpha Sapphire.
It only writes to:
  sdmc:/3ds/PokeBank-CFW/bank.dat

TEST 1 - BANK
-------------
1. Launch PokeBank-CFW.
2. Confirm the Bank says verified or created 100 boxes.
3. Move Bank slots with the D-Pad.
4. Change Bank boxes with L/R while BANK has focus.

TEST 2 - ORAS DETECTION
-----------------------
1. Insert Omega Ruby / Alpha Sapphire OR use an installed SD/eShop copy.
2. Launch PokeBank-CFW or press SELECT.
3. The bottom screen should show the game name and Cartridge or SD.
4. The first displayed GAME BOX should normally match the last PC box viewed in-game.

TEST 3 - BOX READING
--------------------
1. With GAME focused, use L/R through several ORAS boxes.
2. Occupied slots show P.
3. Shiny slots show *.
4. Press A on an occupied slot.
5. Check that the species number and PID look sensible.
6. A normal valid Pokemon should show PK6 OK.

If every populated Pokemon says BAD CHECKSUM, stop and report it.

TEST 4 - SAFE DEPOSIT COPY
--------------------------
1. Select an empty Bank destination.
2. Press Y to focus GAME.
3. Select a populated Pokemon.
4. Press X to copy it.
5. Press Y to focus BANK and inspect the destination with A.
6. Close and reopen PokeBank-CFW.
7. Confirm the Bank slot is still populated.
8. Launch ORAS and confirm the original Pokemon is still present.

OVERWRITE SAFETY
----------------
For an occupied Bank destination:
- first X arms overwrite
- second X confirms overwrite

DO NOT TEST YET
---------------
- withdrawal into ORAS
- deleting from ORAS
- changing the ORAS save
- cross-generation conversion

REPORT
------
Please report:
- 3DS model
- Omega Ruby or Alpha Sapphire
- cartridge or SD install
- whether detection worked
- whether correct populated slots appeared
- whether PK6 checksums showed OK
- whether a copied Pokemon persisted after reopening PokeBank-CFW
