/******************************************* includes *******************************************/
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/******************************************* defines *******************************************/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY, 
	END_KEY, 
	PAGE_UP, 
	PAGE_DOWN
};

/******************************************* data *******************************************/
struct editorConfig
{
	int cx, cy;
	struct termios orig_termios;
	int screenrows;
	int screencols;
};

struct editorConfig E;
/******************************************* terminal *******************************************/
void die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

void disableRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
	{
		die("tcsetattr");
	}
}

// Switch from canonical mode to raw mode
void enableRawMode()
{
	// read terminal's attibutes into a struck
	if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
	{
		die("tcgetattr");
	}
	// run this function at exit
	atexit(disableRawMode);
	struct termios raw = E.orig_termios;
	// turn off the ECHO flag
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST); 
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	// set again the terminal's attributes
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
	{
		die("tcgetattr");
	}
}

int editorReadKey()
{
	int nread;
	char c;
	// Wait until i read a character 
	while((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if(nread == -1 && errno != EAGAIN)
		{
			die("read");
		}
	}

	if(c == '\x1b')
	{
		char seq[3];

		if(read(STDIN_FILENO, &seq[0], 1) != 1)
		{
			return '\x1b';
		}
		if(read(STDIN_FILENO, &seq[1], 1) != 1)
		{
			return '\x1b';
		}	
		if(seq[0] == '[')
		{
			if(seq[1] >= '0' && seq[1] <= '9')
			{
				if(read(STDIN_FILENO, &seq[2], 1) != 1)
				{
					return '\x1b';
				}
				if(seq[2] == '~')
				{
					switch(seq[1])
					{
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			else
			{
				switch (seq[1])
				{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		}

		else if(seq[0] == 'O')
		{
			switch(seq[1])
			{
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
		return '\x1b';
	}
	else
	{
		return c;
	}
}

int getCursorPosition(int *rows, int *cols)
{
	char buf[32];
	unsigned int i = 0;
	// Cursor Position Report is printed in stdin
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
	{
		return -1;
	}
	// Read until R and store the CPR into a buffer
	while (i < sizeof(buf) - 1)
	{
		if(read(STDIN_FILENO, &buf[i], 1) != 1)
		{
			break;
		}
		if(buf[i] == 'R')
		{
			break;
		}
		i++;
	}

	buf[i] = '\0';

	if(buf[0] != '\x1b' || buf[1] != '[')
	{
		return -1;
	}
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
	{
		return -1;
	}

	return 0;
}

int getWindowSize(int *rows, int *cols)
{
	struct winsize ws;
	// Use ioctl or getCursorPosition
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
		{
			return -1;
		}
		return getCursorPosition(rows, cols);
	}
	else
	{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** append buffer ***/
struct abuf
{
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;

}

void abFree(struct abuf *ab)
{
	// deallocates the dynamic memory used by abuf
	free(ab->b);
}

/******************************************* output *******************************************/

void editorDrawRows(struct abuf *ab)
{
	int y;
	for(y = 0; y < E.screenrows; y++)
	{
		if (y == E.screenrows / 3)
		{
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION); 
			if(welcomelen > E.screencols)
			{
				welcomelen = E.screencols;
			}
			int padding = (E.screencols - welcomelen) / 2;
			if (padding)
			{
				abAppend(ab, "~", 1);
				padding--;
			}
			while(padding--)
			{
				abAppend(ab, " ", 1);
			}
			abAppend(ab, welcome, welcomelen);
		}
		else
		{
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if(y < E.screenrows - 1)
		{
			abAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen()
{
	struct abuf ab = ABUF_INIT; // equals to struct abuf ab = {NULL	, 0};

	abAppend(&ab, "\x1b[?25l", 6); // hide the cursor
	abAppend(&ab, "\x1b[H", 3); // move the cursor to [0;0H

	editorDrawRows(&ab); // call by reference

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6); // hide the cursor

	write(STDOUT_FILENO, ab.b, ab.len); 
	abFree(&ab);
}

/******************************************* input *******************************************/
void editorMoveCursor(int key)
{
	switch(key)
	{
		case ARROW_LEFT:
		{
			if(E.cx != 0)
			{
				E.cx--;
			}
		} break;
		case ARROW_RIGHT:
		{
			if(E.cx != E.screencols - 1)
			{
				E.cx++;
			}
		} break;
		case ARROW_UP:
		{
			if(E.cy != 0)
			{
				E.cy--;
			}
		} break;
		case ARROW_DOWN:
		{
			if(E.cy != E.screenrows - 1)
			{
				E.cy++;
			}
		} break;
	}
}

void editorProcessKeypress()
{
	int c = editorReadKey();

	switch(c)
	{
		case CTRL_KEY('q'):
		{
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
		} break;

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
		{
			int times = E.screenrows;
			while(times--)
			{
				editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
		}

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

/******************************************* init *******************************************/

void initEditor()
{
	E.cx = 0;
	E.cy = 0;

	if(getWindowSize(&E.screenrows, &E.screencols) == -1)
	{
		die("getWindowSize");
	}
}

int main(int argc, char **argv)
{
	// Change some terminal attributes
	enableRawMode();
	// Determines the screen size
	initEditor();

	while(1)
	{
		// Clears the screen / draws tildes 
		editorRefreshScreen();
		// Waits until reads a key
		editorProcessKeypress();
		
	}

	return 0;
}
