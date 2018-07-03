#if (defined TILES || defined _WIN32 || defined WINDOWS)
#include "animation.h"
#include "catacharset.h"
#include "color.h"
#include "cursesport.h"
#include "cursesdef.h"
#include "game_ui.h"
#include "output.h"

#include <cstring> // strlen
#include <stdexcept>

/**
 * Whoever cares, btw. not my base design, but this is how it works:
 * In absent of a native curses library, this is a simple implementation to
 * store the data that would be given to the curses system.
 * It is later displayed using code in sdltiles.cpp. This file only contains
 * the curses interface.
 *
 * The struct WINDOW is the base. It acts as the normal curses window, having
 * width/height, current cursor location, current coloring (background/foreground)
 * and the actual text.
 * The text is split into lines (curseline), which contains cells (cursecell).
 * Each cell has individual foreground and background, and a character. The
 * character is an UTF-8 encoded string. It should be one or two console cells
 * width. If it's two cells width, the next cell in the line must be completely
 * empty (the string must not contain anything). Also the last cell of a line
 * must not contain a two cell width string.
 */

//***********************************
//Globals                           *
//***********************************

catacurses::window catacurses::stdscr;
std::array<cata_cursesport::pairs, 100> cata_cursesport::colorpairs;   //storage for pair'ed colored

static bool wmove_internal( const catacurses::window &win_, const int y, const int x )
{
    if( !win_ ) {
        return false;
    }
    cata_cursesport::WINDOW &win = *win_.get<cata_cursesport::WINDOW>();
    if( x >= win.width ) {
        return false;
    }
    if( y >= win.height ) {
        return false;
    }
    if( y < 0 ) {
        return false;
    }
    if( x < 0 ) {
        return false;
    }
    win.cursorx = x;
    win.cursory = y;
    return true;
}

//***********************************
//Pseudo-Curses Functions           *
//***********************************

catacurses::window catacurses::newwin( int nlines, int ncols, int begin_y, int begin_x )
{
    if (begin_y < 0 || begin_x < 0) {
        return window(); //it's the caller's problem now (since they have logging functions declared)
    }

    // default values
    if (ncols == 0) {
        ncols = TERMX - begin_x;
    }
    if (nlines == 0) {
        nlines = TERMY - begin_y;
    }

    cata_cursesport::WINDOW *newwindow = new cata_cursesport::WINDOW();
    newwindow->x = begin_x;
    newwindow->y = begin_y;
    newwindow->width = ncols;
    newwindow->height = nlines;
    newwindow->inuse = true;
    newwindow->draw = false;
    newwindow->BG = black;
    newwindow->FG = static_cast<base_color>( 8 );
    newwindow->cursorx = 0;
    newwindow->cursory = 0;
    newwindow->line.resize(nlines);

    for (int j = 0; j < nlines; j++) {
        newwindow->line[j].chars.resize(ncols);
        newwindow->line[j].touched = true; //Touch them all !?
    }
    return std::shared_ptr<void>( newwindow, []( void *const w ) { delete static_cast<cata_cursesport::WINDOW *>( w ); } );
}

inline int newline(cata_cursesport::WINDOW *win)
{
    if (win->cursory < win->height - 1) {
        win->cursory++;
        win->cursorx = 0;
        return 1;
    }
    return 0;
}

// move the cursor a single cell, jumps to the next line if the
// end of a line has been reached, also sets the touched flag.
inline void addedchar(cata_cursesport::WINDOW *win)
{
    win->cursorx++;
    win->line[win->cursory].touched = true;
    if (win->cursorx >= win->width) {
        newline(win);
    }
}


//Borders the window with fancy lines!
void catacurses::wborder( const window &win_, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr,
            chtype bl, chtype br)
{
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    if( win == nullptr ) {
        //@todo: log this
        return;
    }
    int i = 0;
    int j = 0;
    int oldx = win->cursorx; //methods below move the cursor, save the value!
    int oldy = win->cursory; //methods below move the cursor, save the value!

    if (ls) {
        for (j = 1; j < win->height - 1; j++) {
            mvwaddch(win_, j, 0, ls);
        }
    } else {
        for (j = 1; j < win->height - 1; j++) {
            mvwaddch(win_, j, 0, LINE_XOXO);
        }
    }

    if (rs) {
        for (j = 1; j < win->height - 1; j++) {
            mvwaddch(win_, j, win->width - 1, rs);
        }
    } else {
        for (j = 1; j < win->height - 1; j++) {
            mvwaddch(win_, j, win->width - 1, LINE_XOXO);
        }
    }

    if (ts) {
        for (i = 1; i < win->width - 1; i++) {
            mvwaddch(win_, 0, i, ts);
        }
    } else {
        for (i = 1; i < win->width - 1; i++) {
            mvwaddch(win_, 0, i, LINE_OXOX);
        }
    }

    if (bs) {
        for (i = 1; i < win->width - 1; i++) {
            mvwaddch(win_, win->height - 1, i, bs);
        }
    } else {
        for (i = 1; i < win->width - 1; i++) {
            mvwaddch(win_, win->height - 1, i, LINE_OXOX);
        }
    }

    if (tl) {
        mvwaddch(win_, 0, 0, tl);
    } else {
        mvwaddch(win_, 0, 0, LINE_OXXO);
    }

    if (tr) {
        mvwaddch(win_, 0, win->width - 1, tr);
    } else {
        mvwaddch(win_, 0, win->width - 1, LINE_OOXX);
    }

    if (bl) {
        mvwaddch(win_, win->height - 1, 0, bl);
    } else {
        mvwaddch(win_, win->height - 1, 0, LINE_XXOO);
    }

    if (br) {
        mvwaddch(win_, win->height - 1, win->width - 1, br);
    } else {
        mvwaddch(win_, win->height - 1, win->width - 1, LINE_XOOX);
    }

    //methods above move the cursor, put it back
    wmove(win_, oldy, oldx);
    wattroff(win_, c_white);
}

void catacurses::mvwhline( const window &win, int y, int x, chtype ch, int n )
{
    wattron(win, BORDER_COLOR);
    if (ch) {
        for (int i = 0; i < n; i++) {
            mvwaddch(win, y, x + i, ch);
        }
    } else {
        for (int i = 0; i < n; i++) {
            mvwaddch(win, y, x + i, LINE_OXOX);
        }
    }
    wattroff(win, BORDER_COLOR);
}

void catacurses::mvwvline(const window &win, int y, int x, chtype ch, int n)
{
    wattron(win, BORDER_COLOR);
    if (ch) {
        for (int j = 0; j < n; j++) {
            mvwaddch(win, y + j, x, ch);
        }
    } else {
        for (int j = 0; j < n; j++) {
            mvwaddch(win, y + j, x, LINE_XOXO);
        }
    }
    wattroff(win, BORDER_COLOR);
}

//Refreshes a window, causing it to redraw on top.
void catacurses::wrefresh(const window &win_)
{
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    //@todo: log win == nullptr
    if( win != nullptr && win->draw ) {
        cata_cursesport::curses_drawwindow( win_ );
    }
}

//Refreshes the main window, causing it to redraw on top.
void catacurses::refresh()
{
    return wrefresh(stdscr);
}

void catacurses::wredrawln( const window &/*win*/, int /*beg_line*/, int /*num_lines*/ ) {
    /**
     * This is a no-op for non-curses implementations. wincurse.cpp doesn't
     * use windows console for rendering, and sdltiles.cpp doesn't either.
     * If we had a console-based windows implementation, we'd need to do
     * something here to force the line to redraw.
     */
}

// Get a sequence of Unicode code points, store them in target
// return the display width of the extracted string.
inline int fill(const char *&fmt, int &len, std::string &target)
{
    const char *const start = fmt;
    int dlen = 0; // display width
    const char *tmpptr = fmt; // pointer for UTF8_getch, which increments it
    int tmplen = len;
    while( tmplen > 0 ) {
        const uint32_t ch = UTF8_getch(&tmpptr, &tmplen);
        // UNKNOWN_UNICODE is most likely a (vertical/horizontal) line or similar
        const int cw = (ch == UNKNOWN_UNICODE) ? 1 : mk_wcwidth(ch);
        if( cw > 0 && dlen > 0 ) {
            // Stop at the *second* non-zero-width character
            break;
        } else if( cw == -1 && start == fmt ) {
            // First char is a control character: they only disturb the screen,
            // so replace it with a single space (e.g. instead of a '\t').
            // Newlines at the begin of a sequence are handled in printstring
            target.assign( " ", 1 );
            len = tmplen;
            fmt = tmpptr;
            return 1; // the space
        } else if( cw == -1 ) {
            // Control character but behind some other characters, finish the sequence.
            // The character will either by handled by printstring (if it's a newline),
            // or by the next call to this function (replaced with a space).
            break;
        }
        fmt = tmpptr;
        dlen += cw;
    }
    target.assign(start, fmt - start);
    len -= target.length();
    return dlen;
}

// The current cell of the window, pointed to by the cursor. The next character
// written to that window should go in this cell.
// Returns nullptr if the cursor is invalid (outside the window).
inline cata_cursesport::cursecell *cur_cell(cata_cursesport::WINDOW *win)
{
    if( win->cursory >= win->height || win->cursorx >= win->width ) {
        return nullptr;
    }
    return &(win->line[win->cursory].chars[win->cursorx]);
}

//The core printing function, prints characters to the array, and sets colors
inline void printstring(cata_cursesport::WINDOW *win, const std::string &text)
{
    using cata_cursesport::cursecell;
    win->draw = true;
    int len = text.length();
    if( len == 0 ) {
        return;
    }
    const char *fmt = text.c_str();
    // avoid having an invalid cursorx, so that cur_cell will only return nullptr
    // when the bottom of the window has been reached.
    if( win->cursorx >= win->width ) {
        if( newline( win ) == 0 ) {
            return;
        }
    }
    if( win->cursory >= win->height || win->cursorx >= win->width ) {
        return;
    }
    if( win->cursorx > 0 && win->line[win->cursory].chars[win->cursorx].ch.empty() ) {
        // start inside a wide character, erase it for good
        win->line[win->cursory].chars[win->cursorx - 1].ch.assign(" ");
    }
    while( len > 0 ) {
        if( *fmt == '\n' ) {
            if( newline(win) == 0 ) {
                return;
            }
            fmt++;
            len--;
            continue;
        }
        cursecell *curcell = cur_cell( win );
        if( curcell == nullptr ) {
            return;
        }
        const int dlen = fill(fmt, len, curcell->ch);
        if( dlen >= 1 ) {
            curcell->FG = win->FG;
            curcell->BG = win->BG;
            curcell->FS = win->FS;
            addedchar( win );
        }
        if( dlen == 1 ) {
            // a wide character was converted to a narrow character leaving a null in the
            // following cell ~> clear it
            cursecell *seccell = cur_cell( win );
            if (seccell && seccell->ch.empty()) {
                seccell->ch.assign(' ', 1);
            }
        } else if( dlen == 2 ) {
            // the second cell, per definition must be empty
            cursecell *seccell = cur_cell( win );
            if( seccell == nullptr ) {
                // the previous cell was valid, this one is outside of the window
                // --> the previous was the last cell of the last line
                // --> there should not be a two-cell width character in the last cell
                curcell->ch.assign(' ', 1);
                return;
            }
            seccell->FG = win->FG;
            seccell->BG = win->BG;
            seccell->FS = win->FS;
            seccell->ch.erase();
            addedchar( win );
            // Have just written a wide-character into the last cell, it would not
            // display correctly if it was the last *cell* of a line
            if( win->cursorx == 1 ) {
                // So make that last cell a space, move the width
                // character in the first cell of the line
                seccell->ch = curcell->ch;
                curcell->ch.assign(1, ' ');
                // and make the second cell on the new line empty.
                addedchar( win );
                cursecell *thicell = cur_cell( win );
                if( thicell != nullptr ) {
                    thicell->ch.erase();
                }
            }
        }
        if( win->cursory >= win->height ) {
            return;
        }
    }
}

//Prints a formatted string to a window at the current cursor, base function
void catacurses::wprintw(const window &win, const std::string &printbuf )
{
    if( !win ) {
        //@todo: log this
        return;
    }

    return printstring( win.get<cata_cursesport::WINDOW>(), printbuf );
}

//Prints a formatted string to a window, moves the cursor
void catacurses::mvwprintw(const window &win, int y, int x, const std::string &printbuf )
{
    if( !wmove_internal( win, y, x ) ) {
        return;
    }
    return printstring(win.get<cata_cursesport::WINDOW>(), printbuf);
}

//Resizes the underlying terminal after a Window's console resize(maybe?) Not used in TILES
void catacurses::resizeterm()
{
    game_ui::init_ui();
}

//erases a window of all text and attributes
void catacurses::werase(const window &win_)
{
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    if( win == nullptr ) {
        //@todo: log this
        return;
    }

    for (int j = 0; j < win->height; j++) {
        win->line[j].chars.assign(win->width, cata_cursesport::cursecell());
        win->line[j].touched = true;
    }
    win->draw = true;
    wmove(win_, 0, 0);
    //    wrefresh(win);
    handle_additional_window_clear( win );
}

//erases the main window of all text and attributes
void catacurses::erase()
{
    return werase(stdscr);
}

//pairs up a foreground and background color and puts it into the array of pairs
void catacurses::init_pair( const short pair, const base_color f, const base_color b )
{
    cata_cursesport::colorpairs[pair].FG = f;
    cata_cursesport::colorpairs[pair].BG = b;
}

//moves the cursor in a window
void catacurses::wmove( const window &win_, int y, int x)
{
    if( !wmove_internal( win_, y, x ) ) {
        return;
    }
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    win->cursorx = x;
    win->cursory = y;
}

//Clears the main window     I'm not sure if its suppose to do this?
void catacurses::clear()
{
    return wclear(stdscr);
}

//adds a character to the window
void catacurses::mvwaddch(const window &win, int y, int x, const chtype ch)
{
    if( !wmove_internal( win, y, x ) ) {
        return;
    }
    return waddch(win, ch);
}

//clears a window
void catacurses::wclear( const window &win_)
{
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    werase(win_);
    if( win == nullptr ) {
        //@todo: log this
        return;
    }

    for (int i = 0; i < win->y && i < stdscr.get<cata_cursesport::WINDOW>()->height; i++) {
        stdscr.get<cata_cursesport::WINDOW>()->line[i].touched = true;
    }
}

//gets the max x of a window (the width)
int catacurses::getmaxx(const window &win)
{
    return win ? win.get<cata_cursesport::WINDOW>()->width : 0;
}

//gets the max y of a window (the height)
int catacurses::getmaxy(const window &win)
{
    return win ? win.get<cata_cursesport::WINDOW>()->height : 0;
}

//gets the beginning x of a window (the x pos)
int catacurses::getbegx(const window &win)
{
    return win ? win.get<cata_cursesport::WINDOW>()->x : 0;
}

//gets the beginning y of a window (the y pos)
int catacurses::getbegy(const window &win)
{
    return win ? win.get<cata_cursesport::WINDOW>()->y : 0;
}

//gets the current cursor x position in a window
int catacurses::getcurx(const window &win)
{
    return win ? win.get<cata_cursesport::WINDOW>()->cursorx : 0;
}

//gets the current cursor y position in a window
int catacurses::getcury(const window &win)
{
    return win ? win.get<cata_cursesport::WINDOW>()->cursory : 0;
}

void catacurses::curs_set(int)
{
}

void catacurses::wattron( const window &win_, const nc_color &attrs )
{
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    if( win == nullptr ) {
        //@todo: log this
        return;
    }

    int pairNumber = attrs.to_color_pair_index();
    win->FG = cata_cursesport::colorpairs[pairNumber].FG;
    win->BG = cata_cursesport::colorpairs[pairNumber].BG;
    if (attrs.is_bold()) {
        win->FG = static_cast<base_color>( win->FG + 8 );
    }
    if (attrs.is_blink()) {
        win->BG = static_cast<base_color>( win->BG + 8 );
    }
    if (attrs.is_italic()) {
        win->FS.set( cata_cursesport::FS_ITALIC );
    }
    if (attrs.is_underline()) {
        win->FS.set( cata_cursesport::FS_UNDERLINE );
    }
}

void catacurses::wattroff(const window &win_, int)
{
    cata_cursesport::WINDOW *const win = win_.get<cata_cursesport::WINDOW>();
    if( win == nullptr ) {
        //@todo: log this
        return;
    }

    win->FG = static_cast<base_color>( 8 );                                //reset to white
    win->BG = black;                                //reset to black
    win->FS.reset( cata_cursesport::FS_BOLD );
    win->FS.reset( cata_cursesport::FS_ITALIC );
    win->FS.reset( cata_cursesport::FS_UNDERLINE );
}

void catacurses::waddch(const window &win, const chtype ch)
{
    char charcode;
    charcode = ch;

    switch (ch) {       //LINE_NESW  - X for on, O for off
    case LINE_XOXO:
        charcode = LINE_XOXO_C;
        break;
    case LINE_OXOX:
        charcode = LINE_OXOX_C;
        break;
    case LINE_XXOO:
        charcode = LINE_XXOO_C;
        break;
    case LINE_OXXO:
        charcode = LINE_OXXO_C;
        break;
    case LINE_OOXX:
        charcode = LINE_OOXX_C;
        break;
    case LINE_XOOX:
        charcode = LINE_XOOX_C;
        break;
    case LINE_XXOX:
        charcode = LINE_XXOX_C;
        break;
    case LINE_XXXO:
        charcode = LINE_XXXO_C;
        break;
    case LINE_XOXX:
        charcode = LINE_XOXX_C;
        break;
    case LINE_OXXX:
        charcode = LINE_OXXX_C;
        break;
    case LINE_XXXX:
        charcode = LINE_XXXX_C;
        break;
    default:
        charcode = (char)ch;
        break;
    }
    char buffer[2] = { charcode, '\0' };
    return printstring( win.get<cata_cursesport::WINDOW>(), buffer );
}

static constexpr int A_BLINK = 0x00000800; /* Added characters are blinking. */
static constexpr int A_BOLD = 0x00002000; /* Added characters are bold. */
static constexpr int A_ITALIC = 0x00800000; /* Added characters are italic. */
static constexpr int A_UNDERLINE = 0x00000200; /* Added characters are underline. */
static constexpr int A_COLOR = 0x037e0000; /* Color bits */

nc_color nc_color::from_color_pair_index( const int index )
{
    return nc_color( ( index << 17 ) & A_COLOR );
}

int nc_color::to_color_pair_index() const
{
    return ( attribute_value & A_COLOR ) >> 17;
}

nc_color nc_color::bold() const
{
    return nc_color( attribute_value | A_BOLD );
}

bool nc_color::is_bold() const
{
    return attribute_value & A_BOLD;
}

nc_color nc_color::blink() const
{
    return nc_color( attribute_value | A_BLINK );
}

bool nc_color::is_blink() const
{
    return attribute_value & A_BLINK;
}

nc_color nc_color::italic() const
{
    return nc_color( attribute_value | A_ITALIC );
}

bool nc_color::is_italic() const
{
    return attribute_value & A_ITALIC;
}

nc_color nc_color::underline() const
{
    return nc_color( attribute_value | A_UNDERLINE );
}

bool nc_color::is_underline() const
{
    return attribute_value & A_UNDERLINE;
}

#endif
