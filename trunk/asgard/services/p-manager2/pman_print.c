
#include "types.h"
#include "pman_print.h"
#include <drivers/screen/screen.h>
#include "helpers.h"

INT32 line = 0;
INT32 color = 7;
char buff[256];
int debug_cont = 0;

void pman_print(char *str, ...)
{
	INT32 *args = (INT32*)(&str + 1);

	vsprintf(buff, str, args);
	string_print(buff, 80*2*line, color);
	line++;
	if(line >= 24) 
	{
		line = 0;
		color++;
	}
}

void pman_print_dbg(char *str, ...)
{
	INT32 *args = (INT32*)(&str + 1);
	debug_cont = 0;
	vsprintf(buff, str, args);
	string_print(buff, 80*2*line, color);
	line++;
	if(line >= 24) 
	{
		line = 0;
		color++;
	}	
}

void pman_print_and_stop(char *str, ...)
{
	INT32 *args = (INT32*)(&str + 1);

	vsprintf(buff, str, args);
	string_print(buff, 80*2*line, 12);
	STOP;
}

void pman_print_clr(int next_color)
{
	int i;
	for (i = 0; i<(24*80); i++) {
		string_print (" ", 2*i, 7);
	}

	line=0;
	color = next_color;
}

void pman_print_set_color(int c)
{
	color = c;
}
