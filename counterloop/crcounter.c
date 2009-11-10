/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 * Changelog:
 *	Mar 23, 2009: rename and incorporate into cr_tests.
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define FNAM "counter_out"
int main(int argc, char *argv[])
{
	int cnt=0;
	FILE *f;
	char fnam[200];
	int i;

	if (argc > 1)
		snprintf(fnam, 200, "%s/%s", argv[1], FNAM);
	else
		snprintf(fnam, 200,  "%s", FNAM);

	for (i=0; i<100; i++)
		close(i);
	f = fopen(fnam, "r");
	if (!f) {
		cnt = 1;
		f = fopen(fnam, "w");
		if (!f)
			return 1;
		fprintf(f, "%d", ++cnt);
		fclose(f);
	} else {
		fscanf(f, "%d", &cnt);
		fclose(f);
	}
	for (;;) {
		sleep(3);
		f = fopen(fnam, "w");
		if (!f)
			return 1;
		fprintf(f, "%d", ++cnt);
		fclose(f);
	}
	return 0;
}
