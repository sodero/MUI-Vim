-----------------
What's this?
-----------------
A port of Vim including a MUI GUI.

-----------------
Known limitations
-----------------
Vim currently can't detect the terminal size when used
in text mode in MUI Shell. When using the standard shell
Vim can detect the size on startup, but if you resize the
window after startup you will need to refresh Vim using
Ctrl + l to make things look the way they should.

AROS currently seems to ignore the TxSpacing member of
struct RastPort. This results in graphical glitches when
using anything but non smearing bitmaps fonts. Also, on
AROS, the installer is broken due to the incompleteness
of the AROS installer utility.
