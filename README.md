# Pocket Jumper

A minimal native PSP homebrew platformer written in C with PSPSDK.

## Features

- PSP D-pad movement and Cross-button jumping
- Scrolling platform level
- Collectible coins
- Win and fall/retry states
- No external graphics or audio assets required

## Build

Install PSPDEV and PSPSDK using the official instructions:

- https://pspdev.github.io/
- https://github.com/pspdev/pspdev

Make sure PSPDEV and PSPSDK are installed and available in your `PATH`.

Build the game from the project root:

    make

The resulting PSP homebrew executable will be located at:

    build/EBOOT.PBP

## Run in PPSSPP

Open `build/EBOOT.PBP` in PPSSPP.

To run the game on a real PSP, copy the executable to the following path on your memory stick:

    PSP/GAME/PocketJumper/EBOOT.PBP

Create the `PocketJumper` directory if it does not exist.

## Clean

Remove all generated build files:

    make clean

## Run in PPSSPP

1. Open PPSSPP.
2. Choose **Load** / **Browse** (wording varies by version).
3. Open the project's `EBOOT.PBP`.

Alternatively, copy the file to a memory stick or a mounted PSP storage directory:

```text
PSP/GAME/PocketJumper/EBOOT.PBP
```

Then launch it from the PSP's Game menu. Real PSP hardware may require compatible custom firmware to run homebrew.

## Controls

- D-pad Left / Right: move
- Cross (`X`): jump
- Start: restart after winning or falling
- Home: exit the application in PPSSPP

## Notes

This is a deliberately small starter project. It uses simple colored geometry rather than textures, and the HUD uses bars instead of fonts. Treat it as a learning base, then add sprite art, sound, menus, and level data.
