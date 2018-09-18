/******************************************* includes *******************************************/
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/******************************************* defines *******************************************/
#define CTRL_KEY(k) ((k) & 0x1f)

/******************************************* data *******************************************/
struct termios orig_termios;


/******************************************* terminal *******************************************/
void die(const char *s)
{
	perror(s);
	exit(1);
}

void disableRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
	{
		die("tcsetattr");
	}
}

// Switch from canonical mode to raw mode
void enableRawMode()
{
	// read terminal's attibutes into a struck
	if(tcgetattr(STDIN_FILENO, &orig_termios) == -1)
	{
		die("tcgetattr");
	}
	// run this function at exit
	atexit(disableRawMode);
	struct termios raw = orig_termios;
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
	while((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if(nread == -1 && errno != EAGAIN)
		{
			die("read");
		}
	}
	return c;
}

/******************************************* output *******************************************/

void editorRefreshScreen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H",3);
}

/******************************************* input *******************************************/

void editorProcessKeypress()
{
	char c = editorReadKey();

	switch(c)
	{
		case CTRL_KEY('q'):
		{
			exit(0);
		} break;
	}
}

/******************************************* init *******************************************/

int main(int argc, char **argv)
{

	enableRawMode();

	while(1)
	{
		editorRefreshScreen();
		editorProcessKeypress();
	}
	
	return 0;
}
