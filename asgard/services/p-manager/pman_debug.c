

#include "pmanager_internals.h"

int line = 0;
int color = 2;

void pman_print(char *str, ...)
{
	int *args = (int*)(&str + 1);

	char buff[256];
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
	int *args = (int*)(&str + 1);

	char buff[256];
	vsprintf(buff, str, args);
	string_print(buff, 80*2*line, 12);
	STOP;
}


