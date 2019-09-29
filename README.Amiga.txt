Building Vim on OS4, AROS and MorphOS
---------------------------------------------------------------------------------------------------------
Full MUI version:
- Debug binary using gcc: Invoke 'make CC=gcc DEBUG=yes'
- Release binary using gcc: Invoke 'make CC=gcc'
- Aminet LhA archive and README: Invoke 'make dist CC=gcc'. Artefacts in 'dist/tmp'.

Console only 'tiny', 'small', 'normal', 'big' or 'huge' Vim:
- The same as above but with BUILD set to one of the Vim build types. E.g 'make CC=gcc BUILD=tiny'.

---------------------------------------------------------------------------------------------------------
* Please note that MUI builds are always built as 'huge'.
* If you're cross-compiling set CC to whatever compiler you're using.
---------------------------------------------------------------------------------------------------------
