/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 * Changelog:
 *	Mar 23, 2009: rename and incorporate into cr_tests.
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
	int cnt=0;
	FILE *f;
	char fnam[20];
	int i;

	for (i=0; i<100; i++)
		close(i);
	f = fopen("counter_out", "r");
	if (!f) {
		cnt = 1;
		f = fopen("counter_out", "w");
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
		f = fopen("counter_out", "w");
		if (!f)
			return 1;
		fprintf(f, "%d", ++cnt);
		fclose(f);
	}
	return 0;
}
