/************************************************************************
 * tabdiff.c - program to manage changes to tables caused by test
 * scripts.
 *
 * Objectives:
 * ===========
 * To provide a data extraction/restore facility that:
 *    -  Is reasonably quick
 *    -  Looks quite clever (therefore, do not want lots of
 *       separate statements; put a couple of statements up front,
 *       and then array-process the rest)
 *    -  Allows you to:
 *       -  Store table versions
 *       -  Restore table versions
 *    -  Roll-forwards to a subsequent version
 *    -  Roll-backwards to the baseline
 *
 * So, the chosen vehicle is a data-file with perhaps a couple of statements
 * up front, and data beyond.
 *
 * A file is processed by:
 *    -  Recognising the SQL statements
 *    -  Processing the data after it, array-processing it.
 *
 * Parameters
 * ==========
 * Options:
 * -  -l (Long Column Length; default 240)
 * -  -a (Array size; default 100)
 * -  -d (Base directory; default .)
 * -  -s (Scramble column)
 * -  -t (Loader terminator character)
 * -  -h (Output a help message)
 * -  -o Operation. One of:
 *     -  SNAPSHOT; get the current version of the table onto disk
 *     -  DUMP; get the current version of the table more human-friendlyly
 *     -  BASELINE; create a shadow table copy, with the name prefixed
 *        by the ID (see below).
 *     -  DIFFERENCE; compute the differences (add/delete) between the
 *        current and SNAPSHOT tables, and remove the BASELINE
 *     -  FORWARDS; apply the forward changes
 *     -  BACKWARDS; apply the backwards changes
 *     -  RESTORE; zap the table(s) and re-instate a snapshot.
 *     -  LOADER; re-write the table(s) in SQL*Loader format
 *     -  EXECUTE; execute an arbitrary SQL statement stream silently.
 *     -  FLAT; execute arbitrary SQL statements and produce flat file output
 *     -  MERGE; compute the differences between two flat files and generate
 *        SQL to apply them to a database
 *
 * Arguments:
 * 1 - User ID/Password (always)
 * 2 - ID (becomes part of the filenames)
 * 3 - n Tables to work on.
 *     
 * Processing
 * ==========
 * Recognise the options.
 *
 * Multiple -o options are allowed, but the number of
 * sensible combinations is limited. Note that some operations would
 * effectively invalidate some of the files. Note also that, as described
 * here, a SNAPSHOT may be the before or the after: it is up to the
 * the user to distinguish. 
 *
 * SNAPSHOT and BASELINE can go together, in any order.
 * RESTORE and BASELINE can go together, in any order.
 *
 * RESTORE, DIFFERENCE, FORWARDS can go together.
 *
 * FORWARDS and BACKWARDS can go together, provided that a BASELINE and/or
 * SNAPSHOT intervene. 
 * 
 * DIFFERENCE can be followed by BACKWARDS or RESTORE.
 *
 * RESTORE can be followed by FORWARDS or BACKWARDS
 *
 * EXECUTE can be combined with anything, but must come last.
 *
 * Connect to the database
 *
 * Process the tables, according to the provided rules.
 *
 * Disconnect from the database, and exit
 *
 * Thoughts
 * ========
 * We will not bother to cater for now for funny characters in table
 * or column names, or for LONG data in the tables.
 *
 * An update that touches every row of the table will produce pretty
 * stupid differences, since it works out inserts and deletes only, not
 * possible updates.
 *
 * Duplicate rows will confuse it.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#ifndef MINGW32
#include <sys/param.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef LCC
#include <unistd.h>
#else
#ifndef MAX_PATH
#define MAX_PATH 256
#endif
#endif
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifndef MINGW32
extern int errno;
#endif
#ifdef DO_TIMER
#include <sys/times.h>
static int do_snap;
void snap_times(x)
char * x;
{
static struct tms sav_tms;
struct tms tms;
    if (!do_snap)
        return;
    times(&tms);
    if ( (tms.tms_utime - sav_tms.tms_utime) ||
        (tms.tms_stime - sav_tms.tms_stime) ||
        (tms.tms_cutime - sav_tms.tms_cutime) ||
        (tms.tms_cstime - sav_tms.tms_cstime))
    fprintf(stderr, "%s: User: %d System:%d Child User:%d Child System:%d\n",
        x, (tms.tms_utime - sav_tms.tms_utime),
        (tms.tms_stime - sav_tms.tms_stime),
        (tms.tms_cutime - sav_tms.tms_cutime),
        (tms.tms_cstime - sav_tms.tms_cstime));
    sav_tms = tms;
    return;
}
#endif

int in_arr_size;            /* For array processing                */
#include "tabdiff.h"
int char_long;
char term_char;
static char * work_directory;
static char * id;
/*************************************************************************
 * Static functions defined in this file
 */
static void baseline();
static void snapshot();
static void dump();
static void double_proc();
static void difference();
static void forwards();
static void backwards();
static void restore();
static void loader();
static void merge();
static void execute();
static void flat();
static void proc_file();
static enum tok_id proc_non_sel();
static void con_insert();
static void con_delete();
struct sess_con * con;
int scram_cnt;                /* Count of to-be-scrambled strings */
char * scram_cand[MAX_SCRAM]; /* Candidate scramble patterns */
char * tbuf;
char * tlook;
int tlen;
enum tok_id look_tok;
enum look_status look_status;
/*****************************************************************************
 * Handle unexpected errors
 */
void scarper(file_name,line,message)
char * file_name;
int line;
char * message;
{
static int recursed;

    if (recursed)
        return;
    else
        recursed = 1;
    (void) fprintf(stderr, "Unexpected Error %s,line %d\n",
                   file_name,line);
    perror(message);
    (void) fprintf(stderr, "UNIX Error Code %d\n", errno);
    if (con != (struct sess_con *) NULL)
    {
        dbms_error(con);
        dbms_roll(con);
        dbms_disconnect(con);
    }
    (void) fputs("Program Terminating\n", stderr);
    recursed = 0;
    exit(1);
}
struct opt_fun {
  char * option;
  void (*fun)();
} opt_fun[] = {
{"SNAPSHOT", snapshot},
{"BASELINE", baseline},
{"DIFFERENCE", difference},
{"FORWARDS", forwards},
{"BACKWARDS", backwards},
{"RESTORE", restore},
{"LOADER", loader},
{"DUMP", dump},
{"EXECUTE", execute},
{"FLAT", flat},
{"MERGE", merge},
{(char *) NULL, 0}
};
/***********************************************************************
 * getopt() support
 */
extern int getopt();
extern int optind;
extern char * optarg;
static void opt_print(sts)
int sts;
{
    (void) printf("tabdiff: table difference utility\n\
  You can specify:\n\
  Options:\n\
  -  -l (Long Column Length; default 240)\n\
  -  -a (Array size; default 100)\n\
  -  -d (Base directory; default .)\n\
  -  -s Candidate scramble column sub-string\n\
  -  -t (Loader terminator character)\n\
  -  -h (Output a help message)\n\
  -  -o Operation. One of:\n\
      -  SNAPSHOT; get the current version of the table onto disk\n\
      -  BASELINE; create a shadow table copy, with the name prefixed\n\
         by the ID (see below).\n\
      -  DIFFERENCE; compute the differences (add/delete) between the\n\
         current and SNAPSHOT tables, and remove the BASELINE\n\
      -  FORWARDS; apply the forward changes\n\
      -  BACKWARDS; apply the backwards changes\n\
      -  RESTORE; re-instate a snapshot.\n\
      -  DUMP; dump a table.\n\
      -  EXECUTE; execute SQL with minimal feedback.\n\
      -  FLAT; execute SQL and produce flat output.\n\
      -  MERGE; merge original and updated flat files, execute SQL and produce flat output.\n\
      -  LOADER; convert a snapshot to SQL*Loader format.\n\
      An operation may be abbreviated to the first distinct substring.\n\
 \n\
  Arguments (after the options):\n\
  1 - User ID/Password (always)\n\
  2 - ID (becomes part of the filenames)\n\
  3 - n Tables to work on.\n\
\n");
    exit(sts);
}
/************************************************************************
 * Date format control
 */
static void date_format(dyn)
struct dyn_con * dyn;
{
char buf[4096];
char * x;

    if ((x = getenv("NLS_DATE_FORMAT")) == NULL)
        return;
    (void) sprintf(buf, "alter session set nls_date_format='%s'",
           x);
    dyn->statement = buf;
    prep_dml(dyn);
    if (!(dyn->ret_status))
        exec_dml(dyn);        /* Try to ensure no DML locks hanging around */
    dyn->statement = (char *) NULL;
    return;
}
/***********************************************************************
 * Main Program Starts Here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
int main(argc, argv)
int    argc;
char      *argv[];
{
int i, j;
char * x;
int ch;
int opt_cnt;
struct opt_fun * opt_ptr[ sizeof(opt_fun)/sizeof(struct opt_fun)];
struct opt_fun * chk_ptr;
struct opt_fun * w_ptr;
                          /* The possible number of options; not all possible
                             combinations are sensible */
struct dyn_con * dyn;
    opt_cnt = 0;
    work_directory = ".";
    term_char = '|';
    char_long = 240;
    in_arr_size = 100;
    while ( ( ch = getopt ( argc, argv, "t:s:l:hd:a:o:" ) ) != EOF )
    {
        switch ( ch )
        {
        case 'd' :
/*
 * Directory to look for files
 */
            work_directory = optarg;
            break;
        case 't' :
/*
 * Size to allow for long strings.
 */
            term_char = *optarg;
            break;
        case 'l' :
/*
 * Size to allow for long strings.
 */
            char_long = atoi(optarg);
            if (char_long <= 0)
            {
                fprintf(stderr, "Illegal long string length: %d\n", char_long);
                exit(1);
            }
            break;
        case 's' :
/*
 * Scramble candidates
 */
            if (scram_cnt < MAX_SCRAM)
            {
                scram_cand[scram_cnt] = (char *) malloc(ESTARTSIZE);
                if (re_comp(optarg,scram_cand[scram_cnt++],
                     (long int) ESTARTSIZE) >
                          (char *) ESTARTSIZE)
                {
                    fprintf(stderr, "Invalid Regular Expression: %s\n",optarg);
                    exit(1);
                }
            }
            break;
        case 'o' :
/*
 * Processing options
 */
            if (opt_cnt >= sizeof(opt_fun)/sizeof(struct opt_fun))
            {
                fprintf(stderr, "Too many options: %s\n", optarg);
                exit(1);
            }
 
            for (x = optarg; *x != '\0'; *x++ &= ~(0x20));
                                     /* Convert lower case to upper case */
            if (!(j = strlen(optarg)))
            {
                fputs("Option must be not null\n", stderr);
                exit(1);
            }
            for (w_ptr = opt_fun,
                 chk_ptr = (struct opt_fun *) NULL;
                     w_ptr->option != (char *) NULL;
                           w_ptr ++)
            {
                 if (!strncmp(w_ptr->option,optarg,j))
		 {
                     if (chk_ptr == (struct opt_fun *) NULL)
                         chk_ptr = w_ptr;
                     else
                     {
                         fprintf(stderr, "Option %s is ambiguous\n", optarg);
                         exit(1);
                     }
                 }
            }
            opt_ptr[opt_cnt++] = chk_ptr;
            break;
 
        case 'a' :
/*
 * Array processing size
 */
            in_arr_size = atoi(optarg);
            if (in_arr_size <= 0)
            {
                fprintf(stderr, "Illegal array size: %d\n", in_arr_size);
                exit(1);
            }
            break;

        case 'h' :
            opt_print(0);    /* Does not return */
        default:
        case '?' : /* Default - invalid opt.*/
            (void) fputs("Invalid option; try -h\n", stderr);
            opt_print(1);    /* Does not return */
        }
    }
#ifdef SQLITE3
    in_arr_size = 1;
#endif
    set_def_binds(in_arr_size, in_arr_size);
    if (argc < (optind +3))
    {
        (void) fputs("Insufficient Arguments Supplied\n", stderr);
        opt_print(1);    /* Does not return */
    }
#ifdef DO_TIMER
    do_snap = 1;
    snap_times("Before Database Login");
#endif
    if ((con = dyn_connect(argv[optind++], "tabdiff"))
                       == (struct sess_con *) NULL)
    {
        (void) fprintf(stderr,
               "Failed to log on to database with user/password %s\n",
                  argv[optind - 1]);
        exit(1);    /* Does not return */
    }
#ifdef DO_TIMER
    snap_times("After Database Login");
    do_snap = 0;
#endif
    id = argv[optind++];

    if ((dyn = dyn_init(con, NONSP_CURS)) == (struct dyn_con *) NULL)
        scarper(__FILE__,__LINE__, "Control Structure Allocation Failed");
    date_format(dyn);
/*************************************************************************
 * Main processing : Loop through all the tables for each option
 */
    for (i = optind; i < argc; i++)
       for (j = 0; j < opt_cnt; j++)
          (*((opt_ptr[j])->fun))(dyn,argv[i]);

/************************************************************************
 * Closedown
 */
    dyn_kill(dyn);
    dbms_commit(con);
#ifdef DO_TIMER
    snap_times("After Processing Arguments");
#endif
    dbms_disconnect(con);
#ifdef DO_TIMER
    snap_times("After Disconnecting from the database");
#endif
    exit(0);
}
/************************************************************************
 * Option Processing Routines
 * - Establish a baseline
 */
static void baseline(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char buf[4096];

/*
 * Create the shadow tablename as id+_+table
 * Drop this table regardless
 */
    (void) sprintf(buf, "drop table %s_%s",id,tab);
    dyn->statement = buf;
    prep_dml(dyn);
    if (!(dyn->ret_status))
        exec_dml(dyn);        /* Try to ensure no DML locks hanging around */
/*
 *  Create a new table as create id_table as select * from table;
 *  Return
 */
    (void) sprintf(buf, "create table %s_%s as select * from %s",id,tab,tab);
    dyn->statement = buf;
    prep_dml(dyn);
    if (&(dyn->ret_status))
    {
        (void) fprintf(stderr,
               "create table %s_%s as select * from %s:",id,tab,tab);
        scarper(__FILE__,__LINE__, "Snapshot table create parse failed");
    }
    exec_dml(dyn);        /* Try to ensure no DML locks hanging around */
    dyn->statement = (char *) NULL;
    return;
}
/****************************************************************************
 * Implement the snapshot capability
 */
static void snapshot(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char fname[PATHSIZE];
FILE * fp;

/*
 * Open the output file: work_directory/id_table.tds
 */
    (void) sprintf(fname, "%s/%s_%s.tds",work_directory,id,tab);
    if ((fp = fopen(fname, "wb")) == (FILE *) NULL)
    {
        (void) fprintf(stderr, "%s/%s_%s.tds:",work_directory,id,tab);
        scarper(__FILE__,__LINE__, "Failed to open snapshot file");
    }
/*
 * Write out the truncate statement
 */
#ifdef SYBASE
    (void) fprintf(fp, "truncate table %s\n/\n",tab);
#else
#ifdef SQLITE3
    (void) fprintf(fp, "begin transaction;\n/\ndelete from %s;\n/\n",tab);
#else
    (void) fprintf(fp, "truncate table %s reuse storage\n/\n",tab);
#endif
#endif
/*
 * Process select * from table
 */
    (void) sprintf(fname, "select * from %s",tab);
    dyn->statement = fname;
    prep_dml(dyn);
    exec_dml(dyn);
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
/*
 * Create the insert statement using the returned columns, and write it out
 */
    con_insert(fp,tab,dyn);
/*
 * Write out all the data in a character format that can easily be read back
 * in; make sure that the ' are stuffed.
 */
    res_process(fp,(FILE *) NULL,dyn,form_print,1);
/*
 * Close the file
 */
   (void) fclose(fp);
   dyn->statement = (char *) NULL;
   return;
}
/****************************************************************************
 * Implement the loader capability
 */
static void loader(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
enum tok_id tok_id;
short int i,l;
E2SQLDA * rsdp;
char fname[PATHSIZE];
FILE * fpr;
FILE * fpc;
FILE * fpd;

    (void) sprintf(fname, "%s/%s_%s.tds",work_directory,id,tab);
/*
 * Attempt to open the input file: work_directory/id_table.tds
 */
    if ((fpr = fopen(fname, "rb")) == (FILE *) NULL)
    {
/*
 * Create a normal snapshot if none already exists
 */
        snapshot(dyn,tab);
        if ((fpr = fopen(fname, "rb")) == (FILE *) NULL)
        {
            (void) fputs(fname,stderr);
            putc(':',stderr);
            scarper(__FILE__,__LINE__, "Failed to open snapshot file");
        }
    }
    else
    {
/*
 * Process select * from table
 */
        (void) sprintf(fname, "select * from %s",tab);
        dyn->statement = fname;
        prep_dml(dyn);
        exec_dml(dyn);
        if (dyn->sdp == (E2SQLDA *) NULL)
            desc_sel(dyn);
    }
/*
 * First pass.
 *
 * Process the file of statements and data with a simple parser. We are
 * looking for any columns that are not null.
 */
    tbuf = malloc(WORKSPACE);
    tlook = malloc(WORKSPACE);
    look_status = CLEAR;
    (void) get_tok(fpr);   /* Skip the truncate statement */
    (void) get_tok(fpr);   /* Skip the insert statement */
    dyn->statement = (char *) NULL;
    tok_id = get_tok(fpr);
    rsdp = dyn->sdp;
/*
 * Use the nullability flag to see if we want the column
 */
    for (i = 0; i < rsdp->N; i++)
        rsdp->U[i] = 0;
    dyn->fld_ind = 0;
    while (tok_id != PEOF && tok_id != SQL && tok_id != COMMAND)
    {
        switch(tok_id)
        {
        case FNUMBER:
        case E2FLONG:
        case FDATE:
        case FRAW:
        case FIELD:
            if (dyn->fld_ind >=  rsdp->F)
            {
                fprintf(stderr, "%s:%s\nOffset: %d\n",tab, tbuf, ftell(fpr));
                scarper(__FILE__, __LINE__, 
                         "Syntax error in input, Extra FIELD");
            }
            l = strlen(tbuf);   /* Field Length */
            if (l > 0)
                rsdp->U[dyn->fld_ind] = 1;
            dyn->fld_ind++;
            break;
        case EOR:
            dyn->fld_ind = 0;
            break;
        default:
            {
                fprintf(stderr, "%s:%s\nOffset: %d token: %d\n",tab, tbuf,
                              ftell(fpr), tok_id);
                scarper(__FILE__, __LINE__,  "Syntax error in file");
            }
        }
        fflush(stdout);
        tok_id = get_tok(fpr);
    }
/*
 * Open the Control file: work_directory/id_table.ctl
 */
    (void) sprintf(fname, "%s/%s_%s.ctl",work_directory,id,tab);
    if ((fpc = fopen(fname, "wb")) == (FILE *) NULL)
    {
        (void) fputs(fname,stderr);
        putc(':',stderr);
        scarper(__FILE__,__LINE__, "Failed to open loader control file");
    }
    fputs("options ( silent=feedback, direct=true)\n" , fpc);
    fputs("load data\n" , fpc);
    fputs("infile *\n" , fpc);
    fputs("truncate\n" , fpc);
    fprintf(fpc, "into table %s\n", tab);
    fprintf(fpc, "fields terminated by \"%c\"\n" , term_char);
    fputs("trailing nullcols\n" , fpc);
/*
 * Output all the columns if the table was empty
 */
    for (i = 0, l = 0; i < rsdp->N; i++)
        if (rsdp->U[i] == 1)
        {
            l = 1;
            break;
        }
    if (!l)
    {
        for (i = 0; i < rsdp->N; i++)
            rsdp->U[i] = 1;
        l = 1;
    }
    for (i = 0; i < rsdp->N; i++)
    {
        if (rsdp->U[i] == 1)
        {
            if (l == 1)
            {
                putc('(', fpc);
                l = 0;
            }
            else
                putc(',', fpc);
            fprintf(fpc, "\"%*.*s\" char(%d)\n",
                  rsdp->C[i],rsdp->C[i],rsdp->S[i],rsdp->L[i]);
        }
    }
    fputs(") begindata\n" , fpc);
    (void) fclose(fpc);
/*
 * Open the output file: work_directory/id_table.dat
 */
    (void) sprintf(fname, "%s/%s_%s.dat",work_directory,id,tab);
    if ((fpd = fopen(fname, "wb")) == (FILE *) NULL)
    {
        (void) fputs(fname,stderr);
        putc(':',stderr);
        scarper(__FILE__,__LINE__, "Failed to open loader data file");
    }
/*
 * Re-write the data in SQL*Loader format, omitting columns that are always
 * NULL.
 */
    (void) fseek(fpr,0,0);
    look_status = CLEAR;
    (void) get_tok(fpr);   /* Skip the truncate statement */
    (void) get_tok(fpr);   /* Skip the insert statement */
    tok_id = get_tok(fpr);
    dyn->fld_ind = 0;
    while (tok_id != PEOF && tok_id != SQL && tok_id != COMMAND)
    {
        switch(tok_id)
        {
        case FNUMBER:
        case E2FLONG:
        case FDATE:
        case FRAW:
        case FIELD:
            if (dyn->fld_ind >=  rsdp->F)
            {
                fprintf(stderr, "%s:%s\nOffset: %d\n",tab, tbuf, ftell(fpr));
                scarper(__FILE__, __LINE__, 
                         "Syntax error in input, Extra FIELD");
            }
            l = strlen(tbuf);   /* Field Length */
            if ( rsdp->U[dyn->fld_ind] == 1)
            {
                if (l > 0)
                {
#ifndef SYBASE
                    if ( rsdp->T[dyn->fld_ind] == ORA_DATE)
                    {
                        putc(*tbuf,fpd);
                        putc(*(tbuf+1),fpd);
                        putc('-',fpd);
                        putc(*(tbuf+3),fpd);
                        putc(*(tbuf+4),fpd);
                        putc(*(tbuf+5),fpd);
                        putc('-',fpd);
                        putc(*(tbuf+7),fpd);
                        putc(*(tbuf+8),fpd);
                        putc(*(tbuf+9),fpd);
                        putc(*(tbuf+10),fpd);
                    }
                    else
#endif
                        fputs(tbuf, fpd);
                }
                putc(term_char, fpd);
            }
            dyn->fld_ind++;
            break;
        case EOR:
            putc('\n', fpd);
            dyn->fld_ind = 0;
            break;
        default:
            {
                fprintf(stderr, "%s:%s\nOffset: %d token: %d\n",tab, tbuf,
                              ftell(fpr), tok_id);
                scarper(__FILE__, __LINE__,  "Syntax error in file");
            }
        }
        tok_id = get_tok(fpr);
    }
    free(tbuf);
    free(tlook);
/*
 * Close the files
 */
    (void) fclose(fpr);
    (void) fclose(fpd);
    return;
}
/****************************************************************************
 * Implement the dump capability
 */
static void dump(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char fname[PATHSIZE];
FILE * fp;

/*
 * Open the output file: work_directory/id_table.tds
 */
    (void) sprintf(fname, "%s/%s_%s.tdd",work_directory,id,tab);
    if ((fp = fopen(fname, "wb")) == (FILE *) NULL)
    {
        (void) fprintf(stderr, "%s/%s_%s.tdd:",work_directory,id,tab);
        scarper(__FILE__,__LINE__, "Failed to open dump file");
    }
/*
 * Process select * from table
 */
    (void) sprintf(fname, "select * from %s",tab);
    dyn->statement = fname;
    prep_dml(dyn);
    exec_dml(dyn);
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
/*
 * Write out all the data in a character format that can easily be read back
 * in; make sure that the ' are stuffed.
 */
    res_process(fp,(FILE *) NULL,dyn,col_dump,1);
/*
 * Close the file
 */
   (void) fclose(fp);
   dyn->statement = (char *) NULL;
   return;
}
/*************************************************************************
 * Routine to write data out to two files, insert and delete
 */
static void double_proc(fpi,fpd,dyn,tab)
FILE * fpi;
FILE * fpd;
struct dyn_con * dyn;
char * tab;
{
/*
 * Do a select * on the table to get the columns; not sure that the
 * minus will return what we want.
 */
    get_cols(dyn,tab);
/*
 * Write out a delete statement to .tdb, and an insert to .tdf, and
 * close the cursor.
 */
    con_insert(fpi,tab,dyn);
    con_delete(fpd,tab,dyn);
    dbms_close(dyn);
/*
 * Do select * from table minus select * from id_table
 */
    prep_dml(dyn);
    exec_dml(dyn);
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
/*
 * Write out the data to both files
 */
    res_process(fpi,fpd,dyn,form_print,1);
    return;
}
/*********************************************************************
 * Implement the differential table comparator
 */
static void difference(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char fname[PATHSIZE];
FILE * fpf;
FILE * fpb;
/*
 * Open two output files: work_directory/id_table.td[fb]
 */
    (void) sprintf(fname, "%s/%s_%s.tdf",work_directory,id,tab);
    if ((fpf= fopen(fname, "wb")) == (FILE *) NULL)
    {
        (void) fprintf(stderr, "%s/%s_%s.tdf:",work_directory,id,tab);
        scarper(__FILE__,__LINE__, "Failed to open forwards file");
    }
    fname[strlen(fname) - 1] = 'b';
    if ((fpb= fopen(fname, "wb")) == (FILE *) NULL)
    {
        (void) fprintf(stderr, "%s:", fname);
        scarper(__FILE__,__LINE__, "Failed to open backwards file");
    }
/*
 * Process the records in the new table that are not in the old
 */
    (void) sprintf(fname,
          "select * from %s minus select * from %s_%s",tab,id,tab);
    dyn->statement = fname;
    double_proc(fpf,fpb,dyn,tab);
/*
 * Process the records in the old table that are not in the new
 */
    (void) sprintf(fname,
          "select * from %s_%s minus select * from %s",id,tab,tab);
    dyn->statement = fname;
    double_proc(fpb,fpf,dyn,tab);
/*
 * Close the files
 */
   (void) fclose(fpf);
   (void) fclose(fpb);
/*
 * Drop the snapshot table
 */
    (void) sprintf(fname,
          "drop table %s_%s",id,tab);
    dyn->statement = fname;
    prep_dml(dyn);
    if (dyn->ret_status)
    {
        (void) fprintf(stderr, "%s:", fname);
        scarper(__FILE__,__LINE__, "Snapshot table drop parse failed");
    }
    exec_dml(dyn);
    dyn->statement = (char *) NULL;
   return;
}
/*********************************************************************
 * Implement the rolling of the live table forwards.
 */
static void forwards(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
    proc_file(dyn, tab, "tdf");
    return;
}
/*********************************************************************
 * Implement the rolling of the live table backwards.
 */
static void backwards(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
    proc_file(dyn, tab, "tdb");
    return;
}
/*********************************************************************
 * Implement the restoration of a table snapshot.
 */
static void restore(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
    proc_file(dyn, tab, "tds");
    return;
}
/************************************************************************
 * Process the statements in a file
 *
 * Files have SQL statements optionally interspersed with data. 
 *
 * Loop - until EOF
 * - process a statement
 * - process the corresponding data
 *
 * Commit the work, if successful.
 */
static void proc_file(dyn,tab,ext)
struct dyn_con * dyn;
char * tab;
char * ext;
{
char fname[PATHSIZE];
FILE * fp;
E2SQLDA * tab_cols;
int i;
char **x;
    enum tok_id tok_id;
/*
 * Open the output files: work_directory/id_table.ext
 */
    (void) sprintf(fname, "%s/%s_%s.%s", work_directory,id,tab,ext);
    if ((fp= fopen(fname, "rb")) == (FILE *) NULL)
    {
        fprintf(stderr, "File %s:\n", fname);
        scarper(__FILE__,__LINE__, "Failed to open file to process");
    }
/*
 * Get the data types and lengths needed for the data to be processed
 */
    get_cols(dyn,tab);
    tab_cols = dyn->sdp;       /* Remember the columns for this table */
    dyn->sdp = (E2SQLDA *) NULL; /* Prevent them being de-allocated     */
/*
 * Process the file of statements and data with a simple parser
 */
    tbuf = malloc(WORKSPACE);
    tlook = malloc(WORKSPACE);
    look_status = CLEAR;
    tok_id = get_tok(fp);
    dyn->statement = (char *) NULL;
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
        case SQL:
            tok_id = proc_non_sel(fp,dyn,tab_cols);
            if (dyn->is_sel)
            {
                dyn->sdp = NULL;
                desc_sel(dyn);
                res_process(stdout,(FILE *) NULL,dyn,form_print,1);
                if (dyn->sdp != NULL)
                    sqlclu(dyn->sdp);
                dyn->sdp = NULL;
            }
            break;
        default:
            fprintf(stderr, "File %s: Offset:%d Token:%d Value:%s\n",
                      fname,ftell(fp),tok_id,tbuf);
            scarper(__FILE__, __LINE__,  "Syntax error in file");
        }
    }
    i = tab_cols->N;
    x = tab_cols->S;
    while (i--)
        free(*x++);           /* Select descriptors have malloc()'ed names */
    sqlclu(tab_cols);
    if (dyn->statement != (char *) NULL)
    {
        free(dyn->statement);
        dyn->statement = (char *) NULL;
    }
    free(tbuf);
    free(tlook);
    (void) fclose(fp);
    if (dyn->ret_status == 0)
    {
        dbms_commit(con);
    }
    return;
}
/**************************************************************************
 * Process a statement in the single cursor case.
 */
static enum tok_id proc_any(fp1, fp2, dyn)
FILE *fp1;
FILE *fp2;
struct dyn_con *dyn;
{
enum tok_id tok_id;
char * x = tbuf;

    while(isspace(*x))
        x++;
    if (dyn->statement != (char *) NULL)
        free(dyn->statement);
    dyn->statement = strdup(x);
    prep_dml(dyn);
    tok_id = dyn_exec(fp1, dyn);
    res_process(fp2, (FILE *) NULL, dyn, col_dump, 1);
    free(dyn->statement);
    dyn->statement = (char *) NULL; 
    return tok_id;
}
/**************************************************************************
 * Process a statement in the single cursor case for flat output
 */
static enum tok_id proc_flat(fp1, fp2, dyn)
FILE *fp1;
FILE *fp2;
struct dyn_con *dyn;
{
enum tok_id tok_id;
char * x = tbuf;

    while(isspace(*x))
        x++;
    if (dyn->statement != (char *) NULL)
        free(dyn->statement);
    dyn->statement = strdup(x);
    prep_dml(dyn);
    tok_id = dyn_exec(fp1, dyn);
    if (dyn->sdp != NULL)
    {
        x = col_head_str(dyn->sdp, '}', NULL);
        fputs(x, fp2);
        free(x);
        res_process(fp2, (FILE *) NULL, dyn, flat_print, 1);
    }
    free(dyn->statement);
    dyn->statement = (char *) NULL; 
    return tok_id;
}
/****************************************************************************
 * Implement the statement execution capability
 */
static void execute(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char fname[PATHSIZE];
FILE * fp;
enum tok_id tok_id;
/*
 * Open the output files: work_directory/id_table.ext
 */
    (void) sprintf(fname, "%s/%s_%s.dat", work_directory, id, tab);
    if ((fp= fopen(fname, "wb")) == (FILE *) NULL)
    {
        fprintf(stderr, "File %s:\n",fname);
        scarper(__FILE__,__LINE__, "Failed to open output file");
    }
/*
 * Process the stream of statements and data with a simple parser
 */
    tbuf = malloc(WORKSPACE);
    tlook = malloc(WORKSPACE);
    look_status = CLEAR;
    tok_id = get_tok(stdin);
    dyn->statement = (char *) NULL;
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
        case SQL:
            tok_id = proc_any(stdin, fp, dyn);
            break;
        default:
            fprintf(stderr, "Token:%d Value:%s\n", tok_id, tbuf);
            scarper(__FILE__, __LINE__,  "Syntax error in file");
        }
    }
    if (dyn->statement != (char *) NULL)
    {
        free(dyn->statement);
        dyn->statement = (char *) NULL;
    }
    free(tbuf);
    free(tlook);
    (void) fclose(fp);
    if (dyn->ret_status == 0)
        dbms_commit(con);
    return;
}
/****************************************************************************
 * Implement the statement flat output capability
 */
static void flat(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char fname[PATHSIZE];
FILE * fp;
enum tok_id tok_id;
/*
 * Open the output files: work_directory/id_table.ext
 */
    (void) sprintf(fname, "%s/%s_%s.dat", work_directory, id, tab);
    if ((fp= fopen(fname, "wb")) == (FILE *) NULL)
    {
        fprintf(stderr, "File %s:\n",fname);
        scarper(__FILE__,__LINE__, "Failed to open output file");
    }
/*
 * Process the stream of statements and data with a simple parser
 */
    tbuf = malloc(WORKSPACE);
    tlook = malloc(WORKSPACE);
    look_status = CLEAR;
    tok_id = get_tok(stdin);
    dyn->statement = (char *) NULL;
    while (tok_id != PEOF)
    {
        switch(tok_id)
        {
        case SQL:
            tok_id = proc_flat(stdin, fp, dyn);
            break;
        default:
            fprintf(stderr, "Token:%d Value:%s\n", tok_id, tbuf);
            scarper(__FILE__, __LINE__,  "Syntax error in file");
        }
    }
    if (dyn->statement != (char *) NULL)
    {
        free(dyn->statement);
        dyn->statement = (char *) NULL;
    }
    free(tbuf);
    free(tlook);
    (void) fclose(fp);
    if (dyn->ret_status == 0)
        dbms_commit(con);
    return;
}
#define EOF_FDB 0x1
#define EOF_SDB 0x2
#define HIGH_FDB 0x4
#define LOW_FDB 0x8
#define MATCH_FDB 0x10
/****************************************************************************
 * Implement the merge flat files to generate update SQL. This is actually
 * quite specific to SQLITE, because the inserts do not have the OID column in
 * them ... this is handled automatically.
 */
static void merge(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
char fname[PATHSIZE];
char fname1[PATHSIZE];
char fname2[PATHSIZE];
FILE * new_channel;
enum tok_id tok_id;
struct file_control * fcp1;
int ind1;
struct file_control * fcp2;
int ind2;
int merge_flag;
int i;
time_t clck;
char * ins_SQL;
char * upd_SQL;
char * del_SQL;
struct row * keyp;
char *x;
int iud_flag = 0;
/*
 * Open the input files: work_directory/id_table.ext
 */
    (void) sprintf(fname1, "%s/%s_%s.org", work_directory, id, tab);
    (void) sprintf(fname2, "%s/%s_%s.new", work_directory, id, tab);
    fcp1 = new_data_file_control(fname1, NULL);
    fcp2 = new_data_file_control(fname2, NULL);
    if (strcmp(fcp1->content.data.col_defs->rowp,
               fcp2->content.data.col_defs->rowp))
    {
        fprintf(stderr, "%s (%s) has different columns to %s (%s)\n",
                fname1, fcp1->content.data.col_defs->rowp,
                fname2, fcp2->content.data.col_defs->rowp);
        return;
    }
    sort_rows(&(fcp1->content.data), "OID");
    sort_rows(&(fcp2->content.data), "OID");
    ins_SQL =  create_insert_SQL(tab, fcp1->content.data.col_defs);
    keyp = col_defs("OID");
    del_SQL =  create_delete_SQL(tab, keyp);
    (void) sprintf(fname, "%s/%s_%s.upd", work_directory, id, tab);
    if ((new_channel = fopen(fname, "wb")) == (FILE *) NULL)
    {
        fprintf(stderr, "File %s:\n",fname);
        scarper(__FILE__,__LINE__, "Failed to open output file");
    }
#ifdef SQLITE3
    fputs("begin transaction\n/\n", new_channel);
#endif
/******************************************************************************
 * The merge programming logic is as follows:
 * - Get the first record from each channel.
 * - Now do the serial merge.
 *   - No match, dump out the details (including the subsidiary elements)
 *     for the extra one to the output file.  We assemble in memory a block
 *     that can be strcmp()ed, and dumped to the output file, or rendered to
 *     a List View control. The SQLDA structure looks suitable, as we have
 *     functions for adding and accessing the various elements.
 */
    merge_flag = 0;
    ind1 = 0;
    ind2 = 0;
    iud_flag = 0;
    if (fcp1->content.data.recs <= ind1)
        merge_flag |= EOF_FDB;
    if (fcp2->content.data.recs <= ind2)
        merge_flag |= EOF_SDB;
/*******************************************************************************
 *     Main Control; loop - merge the two lists until both are
 *     exhausted
 ******************************************************************************/
    while  ((merge_flag & (EOF_FDB |  EOF_SDB))
            != (EOF_FDB | EOF_SDB))
    {
        if  ((merge_flag & (EOF_FDB | EOF_SDB)) == 0)
        {
/*
 * If the two streams are still alive, compare the OID
 */
        int id_match = strcmp(fcp1->content.data.rows[ind1]->colp[0],
                                fcp2->content.data.rows[ind2]->colp[0]);
#ifdef DEBUG
            fprintf(stderr, "Compare: %d %s %s\n", id_match, 
                      fcp1->content.data.rows[ind1]->colp[0], 
                      fcp2->content.data.rows[ind2]->colp[0]);
#endif
            if (!id_match)
                merge_flag =  MATCH_FDB;
            else
                merge_flag = (id_match > 0) ? HIGH_FDB : LOW_FDB;
#ifdef DEBUG
            printf(" merge_flag: %d\n", merge_flag);
#endif
        }
        else
            merge_flag &= ~(LOW_FDB | HIGH_FDB);
/*
 * Process an delete, an insert or an update
 */
        if (merge_flag & (EOF_SDB | LOW_FDB))
        {
#ifdef DEBUG
            fprintf(stderr, "%s (%s) is unique to %s\n",
                fcp1->content.data.rows[ind1]->colp[0], fcp1->content.data.rows[ind1]->rowp, fname1);
#endif
/*
 * If we have something in the first file that is not in the second it must
 * have been deleted.
 */
            if (iud_flag != -1)
            {
                iud_flag = -1;
                fputs(del_SQL, new_channel);
                fputs("\n/\n", new_channel);
            }
            fputs(fcp1->content.data.rows[ind1]->colp[0], new_channel);
            fputc('\n', new_channel);
            fflush(new_channel);
            ind1++;
            if (ind1 >= fcp1->content.data.recs)
                merge_flag |= EOF_FDB;
        }
        else
        if (merge_flag & (EOF_FDB | HIGH_FDB))
        {
#ifdef DEBUG
            fprintf(stderr, "%s (%s) is unique to %s\n",
                fcp2->content.data.rows[ind2]->colp[0], fcp1->content.data.rows[ind2]->rowp, fname2);
#endif
/*
 * If we have something in the second file that is not in the first; an insert
 */
            if (iud_flag != 1)
            {
                iud_flag = 1;
                fputs(ins_SQL, new_channel);
                fputs("\n/\n", new_channel);
            }
            x = quoterow(fcp2->content.data.rows[ind2], 1);
            fputs(x, new_channel);
            free(x);
            fflush(new_channel);
            ind2++;
            if (ind2 >= fcp2->content.data.recs)
                merge_flag |= EOF_SDB;
        }
        else /* if (merge_flag == MATCH_FDB) */
        {    /* If the records match */
            iud_flag = 0;
#ifdef DEBUG
            fprintf(stderr, "%s (%s) in %s matches %s (%s) in %s\n",
                fcp1->content.data.rows[ind1]->colp[0],
                fcp1->content.data.rows[ind1]->rowp, fname1,
                fcp2->content.data.rows[ind2]->colp[0],
                fcp2->content.data.rows[ind2]->rowp, fname2);
#endif
/*
 * Carry out a detailed comparison of the columns.
 * -    If the columns all match we want to do nothing.
 * -    The update WHERE needs to specify that the columns that were updated
 *      retain their original values.
 */
            fcp1->content.data.cur_row = ind1;
            fcp2->content.data.cur_row = ind2;
            if ((upd_SQL =  create_custom_update_SQL(tab, &fcp1->content.data,
                       &fcp2->content.data)) != NULL)
            {
                fprintf(new_channel, "%s\n/\n", upd_SQL);
                free(upd_SQL);
            }
            ind1++;
            if (ind1 >= fcp1->content.data.recs)
                merge_flag |= EOF_FDB;
            ind2++;
            if (ind2 >= fcp2->content.data.recs)
                merge_flag |= EOF_SDB;
        }
    }
    fclose(new_channel);
    free(ins_SQL);
    free(del_SQL);
    zap_data_file_control(fcp1, NULL);
    zap_data_file_control(fcp2, NULL);
    proc_file(dyn, tab, "upd");
    return;
}
/************************************************************************
 * Process a non-select SQL statement and its related data
 */
static enum tok_id proc_non_sel(fp,dyn, tab_cols)
FILE *fp;
struct dyn_con * dyn;
E2SQLDA * tab_cols;
{
enum tok_id tok_id;

    if (dyn->statement != (char *) NULL)
        free(dyn->statement);
    dyn->statement = malloc(strlen(tbuf + 1));
    strcpy(dyn->statement,tbuf);
    prep_dml(dyn);
    dyn->sdp = tab_cols;
    tok_id = dyn_exec(fp, dyn);
    dyn->sdp = (E2SQLDA *) NULL;
    return tok_id;
}
/**********************************************************************
 *  con_insert()
 *
 *   Construct an insert statement, and write it out to the designated
 *   file.
 */
static void con_insert(fp,tab,d)
FILE *fp;
char * tab;
struct dyn_con * d;
{
int    cnt;
int    cnt_1;
/*
 * We will recognise SQL statements because the first character of the line
 * will not be a '
 */
    (void) fprintf(fp, "insert into %s (\n",tab);
/*
 * Loop - write out the column names
 */
    for ( cnt_1 = d->sdp->F - 1, cnt = 0; cnt < cnt_1; cnt++)
        (void) fprintf(fp, "%.*s,\n",d->sdp->C[cnt],d->sdp->S[cnt]);
             /* Print a column name */
    (void) fprintf(fp, "%.*s) values (\n",d->sdp->C[cnt],d->sdp->S[cnt]);
/*
 * Loop - write out the names of the bind variables
 */
    for ( cnt = 0; cnt < cnt_1; cnt++)
    {
        switch ((int) d->sdp->T[cnt])
        {
#ifdef SQLITE3
        default:
            (void) fprintf(fp, ":%.*s,\n", d->sdp->C[cnt],d->sdp->S[cnt]);
            break;
#else
#ifdef SYBASE
        default:
            (void) fprintf(fp, "@%.*s,\n", d->sdp->C[cnt],d->sdp->S[cnt]);
            break;
#else
        case ORA_DATE:
            (void) fprintf(fp,                /* Date */
                      "to_date(:%.*s,'DD Mon YYYY HH24:MI:SS'),\n",
                                            d->sdp->C[cnt],d->sdp->S[cnt]);
            break;
        case ORA_LONG_VARRAW:
        case ORA_RAW_MLSLABEL:
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            (void) fprintf(fp,                /* Date */
                      "hextoraw(:%.*s),\n",
                                            d->sdp->C[cnt],d->sdp->S[cnt]);
            break;
        default:
            (void) fprintf(fp, ":%.*s,\n", d->sdp->C[cnt],d->sdp->S[cnt]);
            break;
#endif
#endif
        }
    }
    switch ((int) d->sdp->T[cnt])
    {
#ifdef SQLITE3
    default:
        (void) fprintf(fp, ":%.*s);\n", d->sdp->C[cnt],d->sdp->S[cnt]);
        break;
#else
#ifdef SYBASE
    default:
        (void) fprintf(fp, "@%.*s)\n", d->sdp->C[cnt],d->sdp->S[cnt]);
        break;
#else
    case ORA_DATE:
        (void) fprintf(fp,                /* Date */
                      "to_date(:%.*s,'DD Mon YYYY HH24:MI:SS'))\n",
                                            d->sdp->C[cnt],d->sdp->S[cnt]);
        break;
    case ORA_RAW_MLSLABEL:
    case ORA_LONG_VARRAW:
    case ORA_RAW:
    case ORA_VARRAW:
    case ORA_LONG_RAW:
        (void) fprintf(fp,                /* Date */
                      "hextoraw(:%.*s))\n",
                                        d->sdp->C[cnt],d->sdp->S[cnt]);
        break;
    default:
        (void) fprintf(fp, ":%.*s)\n", d->sdp->C[cnt],d->sdp->S[cnt]);
        break;
#endif
#endif
    }
/*
 * Write out the end-of-statement line
 */
    (void) fputs("/\n", fp);
    return;
}
/**********************************************************************
 *  con_delete()
 *
 *   Construct a delete statement, and write it out to the designated
 *   file.
 */
static void con_delete(fp,tab,d)
FILE *fp;
char * tab;
struct dyn_con * d;
{
    int    cnt;
    int    cnt_1;
/*
 * We will recognise SQL statements because the first character of the line
 * will not be a '
 */
    (void) fprintf(fp, "delete from %s where\n",tab);
/*
 * Loop - write out the column names and indicator variables
 * We want to match:
 * - Not Null with Not Null
 * - Null with Null
 * - Whilst maximising the chances of using indexes
 */
    for ( cnt_1 = d->sdp->F - 1, cnt = 0; cnt < cnt_1; cnt++)
    {                  /* Print column details */
        if (!(d->sdp->U[cnt]))
        {
#ifdef SYBASE
            (void) fprintf(fp, "(%.*s = @%.*s) and\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
#else
#ifndef SQLITE3
            if (d->sdp->T[cnt] == ORA_DATE)
                (void) fprintf(fp,                /* Date */
                   "(%.*s = to_date(:%.*s,'DD Mon YYYY HH24:MI:SS')) and\n",
                         d->sdp->C[cnt],d->sdp->S[cnt],
                         d->sdp->C[cnt],d->sdp->S[cnt]);
            else
#endif
                (void) fprintf(fp, "(%.*s = :%.*s) and\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
#endif
        }
        else     /* might be null */
        {
#ifdef SYBASE
                (void) fprintf(fp, "(%.*s = @%.*s) and\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
#else
#ifndef SQLITE3
            if (d->sdp->T[cnt] == ORA_DATE)
                (void) fprintf(fp,                /* Date */
                "(%.*s = to_date(:%.*s,'DD Mon YYYY HH24:MI:SS') or \n\
(%.*s is null and to_date(:%.*s,'DD Mon YYYY HH24:MI:SS') is null)) and\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
            else
#endif
                (void) fprintf(fp,                  /* Print column details */
                "(%.*s = :%.*s or \n(%.*s is null and :%.*s is null)) and\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
#endif
        }
    }
    if (!(d->sdp->U[cnt]))
    {
#ifdef SYBASE
        (void) fprintf(fp, "(%.*s = @%.*s)\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
#else
#ifndef SQLITE3
        if (d->sdp->T[cnt] == ORA_DATE)
            (void) fprintf(fp,                /* Date */
                   "(%.*s = to_date(:%.*s,'DD Mon YYYY HH24:MI:SS')) and\n",
                         d->sdp->C[cnt],d->sdp->S[cnt],
                         d->sdp->C[cnt],d->sdp->S[cnt]);
        else
#endif
            (void) fprintf(fp,
                "(%.*s = :%.*s)\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
#endif
    }
    else     /* might be null */
    {
#ifdef SYBASE
        (void) fprintf(fp,
            "(%.*s = @%.*s or \n(%.*s is null and @%.*s is null))\n",
             d->sdp->C[cnt],d->sdp->S[cnt],
             d->sdp->C[cnt],d->sdp->S[cnt],
             d->sdp->C[cnt],d->sdp->S[cnt],
             d->sdp->C[cnt],d->sdp->S[cnt]);
#else
#ifndef SQLITE3
        if (d->sdp->T[cnt] == ORA_DATE)
                (void) fprintf(fp,                /* Date */
                "(%.*s = to_date(:%.*s,'DD Mon YYYY HH24:MI:SS') or \n\
(%.*s is null and to_date(:%.*s,'DD Mon YYYY HH24:MI:SS') is null))\n",
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt],
                     d->sdp->C[cnt],d->sdp->S[cnt]);
        else
#endif
            (void) fprintf(fp,
            "(%.*s = :%.*s or \n(%.*s is null and :%.*s is null))\n",
             d->sdp->C[cnt],d->sdp->S[cnt],
             d->sdp->C[cnt],d->sdp->S[cnt],
             d->sdp->C[cnt],d->sdp->S[cnt],
             d->sdp->C[cnt],d->sdp->S[cnt]);
#endif
    }
/*
 * Write out the end-of-statement line
 */
#ifdef SQLITE3
    (void) fputs(";\n", fp);
#endif
    (void) fputs("/\n", fp);
    return;
}
