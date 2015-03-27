set m0 = QSINIT Boot menu help^ ßßßßßßßßßßßßßßßßßßßßß^^
set m1 = Press F9 to enter Apps Menu.^^
set m2 = Press F3 to run shell. To exit from shell type \"exit\" in it.^^
set m3 = Press F5 to call \"SysView\" application.^^
set m4 = Press F7 to list available HDD partitions.^^
set m5 = Press F8 to view internal log.^^
set m6 = Press Ctrl-Enter to copy menu command to shell command line.^^
set m7 = Type \"help\" in shell to help on commands.
set m8 =
set m9 =
set errorlevel = %lines%
if not errorlevel 28 goto m80x25
set m8 = ^^Press Backspace or ESC to go back through menus.^^
set m9 = Press Up/Down in shell to select command from history.
:m80x25
msgbox "Help" "%m0%%m1%%m2%%m3%%m4%%m5%%m6%%m7%%m8%%m9%" CYAN WIDE OK
set m0 =
set m1 =
set m2 =
set m3 =
set m4 =
set m5 =
set m6 =
set m7 =
set m8 =
set m9 =
