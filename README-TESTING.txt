POKEBANK-CFW v0.4-alpha - FIRST ORAS WRITE TEST
===============================================

v0.3 hardware tests all passed.

IMPORTANT:
This is the first build that can WRITE to the ORAS save.
Before every write it makes a full /main backup on the SD card and refuses to continue if backup creation fails.

SAFEST TEST
-----------
1. Use a disposable / test ORAS save if available.
2. Put a normal non-important Pokemon into PokeBank-CFW first.
3. In ORAS choose an EMPTY destination PC slot.
4. Launch PokeBank-CFW.
5. Select the destination GAME slot.
6. Press Y so BANK has focus.
7. Select the Bank Pokemon.
8. Press X once.
9. The app should report Bank->ORAS copied and backup saved.
10. Press B or inspect the destination. It should now show the Pokemon with PK6 OK.
11. Exit PokeBank-CFW normally.
12. Launch ORAS.
13. Confirm the Pokemon is present in the chosen PC slot.
14. Save normally in ORAS, reboot the game, and confirm it is still present.
15. Confirm the original Bank copy still exists.

BACKUP CHECK
------------
Confirm the SD card contains:
  /3ds/PokeBank-CFW/backups/OR-main-latest.bak
or
  /3ds/PokeBank-CFW/backups/AS-main-latest.bak

OVERWRITE TEST
--------------
Do not test occupied-slot overwrite until the empty-slot write above passes.

REPORT
------
- empty-slot withdrawal success/failure
- whether ORAS booted normally afterward
- whether Pokemon persisted after an in-game save/reboot
- whether the Bank copy remained
- any error code shown by PokeBank-CFW