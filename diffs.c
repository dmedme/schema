/**************************************************************************
 * Routines to call the difference engine on two schema blocks.
 *
 * This version tries to avoid the hideous SQLDA representation, in favour
 * of the 'e2dfflib' flat file handling representation, that is much easier to
 * handle, although it takes up twice the space.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1999";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "difflib.h"
/*
 * Functions in this file
 */
#include "tabdiff.h"
/*
 * Compare two counted strings
 */
int mulcmp(l1, p1, l2, p2)
int l1;
char * p1;
int l2;
char * p2;
{
int mflag;
int mlen;
int i;
    if (l1 > l2)
    {
        mlen = l2;
        mflag = 1; 
    }
    else
    {
        mlen = l1;
        if (l1 < l2)
            mflag = -1;
        else
            mflag = 0;
    }
    if (!(i = memcmp(p1, p2, mlen)))
        return mflag;
    else
        return i;
}
/*****************************************************************************
 * Routines to support schema comparison by comparing the contents of two
 * display blocks.
 */
char * disp_gets(buf, len, dp)
char * buf;
int len;
struct disp_con * dp;
{
char * rowp;

    if (dp->so_far >= dp->to_do)
        return NULL;
    rowp = row_str(dp, dp->so_far, '}', '\\', dp->col_flags);
    strncpy(buf, rowp, len);
    buf[len - 1] = '\0';
    free(rowp);
    dp->so_far++;
    return buf;
}
/*****************************************************************************
 * Compare two disp_con structures
 *****************************************************************************
 * In addition to the two disp_con structures, we have to provide.
 * -   A routine, analagous to fgets(), which retrieves an element from disp_con
 * -   The sizeof of its buffer (should be larger than the largest element)
 * -   A routine to compare a pair of the elements
 * -   A routine to retrieve the length of an element
 */
struct diffx_con * disp_compare(dp1, dp2)
struct disp_con * dp1;
struct disp_con * dp2;
{
return diffx_alloc((char *) dp1, (char *) dp2, disp_gets,
                    16384, strcmp, strlen);
}
/******************************************************************************
 * Output the differences between two display blocks.
 */
void diff_disp_rep(ofp, dp1, dp2, dp, output_option)
FILE * ofp;
struct disp_con * dp1;
struct disp_con * dp2;
struct diffx_con * dp;
enum output_option output_option;
{
#ifdef DEBUG
    fputs("First Block\n", stderr);
    fflush(ofp);
    fputs("First Block\n", ofp);
    fflush(ofp);
    disp_print(ofp,dp1);
    fflush(ofp);
    fputs("Second Block\n", ofp);
    fflush(ofp);
    disp_print(ofp,dp2);
    fflush(ofp);
    diagout("diff_disp_rep()\n", ofp, dp);
    fflush(ofp);
#endif
    if (output_option == FULL_LIST )
    {
        if (dp1->title != (char *) NULL)
        {
            fputs(dp1->title, ofp);
            fputc('\n', ofp);
        }
        fputs("I/D ", ofp);
        col_head_print(ofp, dp1->bdp, dp1->col_flags);
    }
    diffrep(ofp, dp, output_option, fputs);
    fflush(ofp);
    return;
}
