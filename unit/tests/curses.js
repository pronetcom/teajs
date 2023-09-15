var curses = require("curses");
var term = new curses.Curses();

row = term.GetRowSize();
column = term.GetColumnSize();
//getmaxyx(stdscr, row, col);		/* get the number of rows and columns */
term.MVPrintw(row / 2, (column - 5) / 2, "aaaaa");
term.Refresh();
//mvprintw(row / 2, (col - strlen(mesg)) / 2, "%s", mesg);
/* print the message at the center of the screen */
//getstr(str);
//mvprintw(LINES - 2, 0, "You Entered: %s", str);
term.Getch();
term.End();