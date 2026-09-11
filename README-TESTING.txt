PokeBank-CFW v0.1 hardware smoke test
=====================================

THIS BUILD DOES NOT TOUCH POKEMON SAVES.

Purpose
-------
1. Confirm the native 3DS application launches.
2. Confirm both screens/input work.
3. Confirm the app can create, write and read its own SD-card storage.

CIA test
--------
1. Copy PokeBank-CFW.cia to your 3DS SD card.
2. Install it with your normal CIA installer (for example FBI).
3. Launch PokeBank-CFW from HOME Menu.
4. The top screen should report:
       SD storage test: PASS
5. Press A to run the test again.
6. Press START to exit.

3DSX test
---------
Copy PokeBank-CFW.3dsx to:
  sdmc:/3ds/PokeBank-CFW/PokeBank-CFW.3dsx
and launch it through Homebrew Launcher.

Expected test file
------------------
The application creates:
  sdmc:/3ds/PokeBank-CFW/storage_test.dat

It only contains a short test signature. It does not scan, open, edit or write any
Pokemon game save in this version.

If it reports FAIL, stop there and report the exact error shown on screen.
