/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 *
 * if i do
 *	runcon -t ckpt_test_1_t ./ckpt
 * then the file->f_cred for ckpt will actually be runcon's
 * before the context switch.  We don't want to have to give
 * the restarter the rights to process:restore unconfined_t,
 * so we'll do
 *	runcon -t ckpt_test_1_t ./wrap ./ckpt
 * so that ckpt is actually opened by a task with type
 * ckpt_test_1_t, so that all file->f_creds are in that context.
 */

int main(int argc, char *argv[])
{
	char *newcmd, **newargv;
	newargv = argv+1;
	newcmd = argv[0];
	return execv(newcmd, newargv);
}
