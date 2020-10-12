RgQuake
==============================

RgQuake is a fork of the modernized version of SDLQuake (https://github.com/ahefner/sdlquake) optimized specifically to be fun to play on a RG350, which is a handheld running OpenDingux. Testing has been done on my RG350p. Thanks to authors of SDLQuake for the groundwork and author of Thenesis Quake for setting pak file conventions.

Note: only the OpenDingux platform is supported, no other OpenDingux devices have been tested but the rg350p, gcw0 should work but probably needs tweaking the controls in the menus to make the single analog work well.

I'll add .desktop files and launch scripts for hipnotic and rogue later.


Changes compared to regular quake
---------------------------------

- Configurable dual or single analog support
- Added cheats menu into options
- Added option for generous autoaim for gamepad use
- Enhanced particle effects
- Added Weapon wheel
- Smoothed out monster movement
- Added option for crosshair
- Added option for non-centered weapon models

Installation
------------

First you need to have a copy of Quake. Shareware version works as well but you'll have less content.
The important game files you'll need are in your Quake installations `id1` directory: pak0.pak (shareware and full version) and pak1.pak (only full version).

To install you can do it in two ways:

1) Installing with OPK
- Put the .opk in your `/media/data/apps`
- Put your id1 directory's .pak files into `/media/data/Quake/id/`.
- If you've installed Thenesis Quake on your device via its OPK, your game files are already in the right place.

OR

2) Putting it on the sdcard
- Put the executable and the bundled id1 directory on your sd card, like `/media/sdcard/Quake/rgquake` and `/media/sdcard/Quake/id1/`.
- Make sure the id1 directory contains the bundled `rg_default.cfg` and  `rg_quake.rc` files as well as your .pak files.
- Use DinguxCmdr to navigate to your Quake directory and execute `rgquake`

If the game files aren't found, the game will inform you. If the game starts but console says something about not finding rg_quake.rc, then the bundled id1 files are not in the right place.

Building
--------

Linux only, cygwin or WSL might work, I've only tested native Ubuntu.

- Make sure you've installed the opendingux toolchain.
- Check that the INCLUDES matches your toolchain paths in the Makefile.
- `make` builds the executable into `./bin`.
- `./make_opk.sh` then copies the binary to opk_data and creates the .opk file.

License
-------

RgQuake is licensed under the GPLv2.  You should have received a copy of the GPLv2 in a file called COPYING in the same directory as this README.  If you did not, contact the distributor from whom you recieved this software for a copy.
