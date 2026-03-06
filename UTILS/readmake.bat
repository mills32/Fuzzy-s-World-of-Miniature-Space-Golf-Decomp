tcc -1 -G -O2 -Ic:\tc\include -ml -c readres.c
tasm /dc /m2 readres_.asm
tlink c0l.obj readres.obj readres_.obj,readres.exe,, cl -Lc:\tc\lib
del *.OBJ
del *.MAP
