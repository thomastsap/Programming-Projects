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

/******************************************* data *******************************************/
struct editorConfig
{
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

char editorReadKey()
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
	
	return c;
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
		abAppend(ab, "~", 1);
		//write(STDOUT_FILENO, "~", 1);

		abAppend(ab, "\x1b[K", 3);
		if(y < E.screenrows - 1)
		{
			abAppend(ab, "\r\n", 2);
			//write(STDOUT_FILENO, "\r\n", 2);
		}
	}
}

void editorRefreshScreen()
{
	struct abuf ab = ABUF_INIT; // equals to struct abuf ab = {NULL	, 0};

	abAppend(&ab, "\x1b[?25l", 6); // hide the cursor
	//abAppend(&ab, "\x1b[2J, 4");
	abAppend(&ab, "\x1b[H", 3);
	//write(STDOUT_FILENO, "\x1b[2J", 4);
	//write(STDOUT_FILENO, "\x1b[H",3);

	editorDrawRows(&ab); // call by reference
	abAppend(&ab, "\x1b[H", 3);
	abAppend(&ab, "\x1b[?25h", 6); // hide the cursor

	write(STDOUT_FILENO, ab.b, ab.len); 
	abFree(&ab);
}

/******************************************* input *******************************************/

void editorProcessKeypress()
{
	char c = editorReadKey();

	switch(c)
	{
		case CTRL_KEY('q'):
		{
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
		} break;
	}
}

/******************************************* init *******************************************/

void initEditor()
{
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
