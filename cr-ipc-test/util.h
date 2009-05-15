/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
#ifndef _UTIL_H
#define _UTIL_H

/* test success */
extern void pass(void);

/* test failure */
extern void fail(const char *msg);

/* Inconclusive, neither pass nor fail */
extern void bail(const char *msg);

#endif /* _UTIL_H */
