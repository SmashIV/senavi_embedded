#include "str_utils.h"

#include <string.h>

char *trim(char *str)
{
	char *end;

	if (!str)
		return NULL;
	while (*str == ' ' || *str == '\t')
		str++;
	end = str + strlen(str);
	while (end > str && (end[-1] == ' ' || end[-1] == '\t' ||
			end[-1] == '\n' || end[-1] == '\r'))
		end--;
	*end = '\0';
	return str;
}
