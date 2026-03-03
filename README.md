Fuzzy's World of Miniature Space Golf Decomp (in progress)
----------------------------------------------------------

This is a cool minigolf game for MS-DOS from 1995 (Pixel Painters Corporation). It requires a 386 and 4MB of RAM, but I plan to recompile for real mode 8088/8086 and 640 KB RAM, (it will probably need a 12 MHz 286 to work OK). Also I'll love to create an SDL/OpenGL port.

At the moment, only some utils were coded:
  - player.c: Plays original loudness tracker music (.DAT) in MS-DOS, and converts patterns, notes and most instrument parameters to impulse tracker (converted .it files require some tweaking and pcm samples, fixed and optimized .it conversions will be added).
  - ppext.c: Pixel painters extractor, extracts files from .RES resources.
  - convert.c: Converts.SPF and .ANI to .GIF. 

FILE TYPES
----------

These are the files contained in the game:
  - ANI: Animation files for background, they contain a 256 color palette, 3 bytes per color (but using 6 bit VGA colors), a first frame with a complete background image, and after that, a sequence of partial images, containing only animated parts. These partial images are uncompressed in real time (I think), most of them are very small and fast to process. All images are ment to be 320x200 pixels.
  - SPF: Static background images and sprite sheets, the same as ANI, but only contain the first 320x200 image.
  - DAT: Music in LOUDNESS tracker format, it is very similar to impulse tracker, but only contains YM3812/OPL2/Adlib instruments.
  - SMP: SFX sounds, just 8 Bit, 11025Hz PCM.
  - TXT: Text data (menus, instructions...).

![screenshot](MENU03A.GIF)
