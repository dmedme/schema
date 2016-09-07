/*
 *    sqldrive.c - Program to drive and collect timings for SQL scripts
 *
 *    Copyright (C) E2 Systems 1993
 *
 *    Timestamps are written when the appropriate events are spotted.
 *
 * Arguments
 * =========
 *   - arg 1 = name of file to output timestamps to
 *   - arg 2 = pid of fdriver
 *   - arg 3 = pid of bundle
 *   - arg 4 = i number within 'rope'
 *   - arg 5 = oracle user/password
 *
 * Signal handling
 * ===============
 * SIGUSR2 - terminate request
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (C) E2 Systems Limited 1993";
#ifndef MINGW32
#include <sys/param.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#include <unistd.h>
#else
#define _toupper toupper
#endif
#include <string.h>
#include <ctype.h>
#ifndef LCC
#include <sys/types.h>
#include <sys/file.h>
#ifdef V32
#include <time.h>
#else
#include <sys/time.h>
#endif
#endif
#ifdef SEQ
#include <fcntl.h>
#include <time.h>
#else
#ifdef ULTRIX
#include <fcntl.h>
#else
#ifdef AIX
#include <fcntl.h>
#else
#ifndef LCC
#include <sys/fcntl.h>
#endif
#endif
#endif
#endif
#include <signal.h>
#include <errno.h>
#include "e2conv.h"
#include "circlib.h"
#include "matchlib.h"
#include "natregex.h"
#include "tabdiff.h"
static void start_timer();
static void take_timer();
static void take_timeout();
static void sqldrive_parse();
static void sqldrive_release();
static void sqldrive_fetch();
static void sqldrive_read();
static void sqldrive_ingres();
static void traf_clear();
static struct word_con * traff_log();
static enum tok_id sqldrive_exec();
void sqldrive_init();
static enum tok_id sqldrive_process();
void sqldrive_quit();
#ifndef SYBASE
#ifndef INGRES
/*
 * Collection of statement execution costs
 */
static void setup_costs();
static void do_costs();
static void get_costs();
static void roll_costs();
static void print_costs();
#endif
#endif
static struct dyn_con * dyn;   /* Global default dynamic statement structure */
#ifdef INGRES
static HASH_CON * all_dyns;    /* Global hash table of statements */
static int cur_port;
#endif
static char * sav_id;

/************************************************************************
 * Dummies; sqldrive doesn't use this functionality
 */
int char_long = 80;
int scram_cnt = 0;                /* Count of to-be-scrambled strings */
char * scram_cand[1];             /* Candidate scramble patterns */
struct sess_con *con;
struct sess_con *all_sess[128];
static int sess_cnt;
/*****************************************************************************
 * Handle unexpected errors
 */
extern int errno;
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
    fflush(stdout);
    (void) fprintf(stderr,"Unexpected Error %s,line %d\n",
                   file_name,line);
    perror(message);
    (void) fprintf(stderr,"UNIX Error Code %d\n", errno);
    if (con != (struct sess_con *) NULL)
        dbms_error(con);
    fflush(stderr);
    recursed = 0;
    return;
}
/***********************************************************************
 * Getopt support
 */
extern int optind;           /* Current Argument counter.      */
extern char *optarg;         /* Current Argument pointer.      */
extern int opterr;           /* getopt() err print flag.       */
extern int errno;
extern struct event_con * curr_event; /* the event we are looking for; used as
                                         a flag to see if scanning or not */
#ifdef LCC
#undef select
#endif
char * tbuf;
char * tlook;
enum tok_id look_tok;
enum look_status look_status;
int tlen;
/*******************
 * Global data
 */
struct ptydrive_glob pg;
static double saved_time;
void siggoaway ()
{
    event_record("F", (struct event_con *) NULL);
    sqldrive_quit_all();
    exit(1);
}
/***********************************************************************
 * Main program starts here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
int main (argc,argv)
    int argc;
    char* argv[];
{
char * start_event;
enum tok_id tok_id;
int i, j;
int ch;
char * id;
char * x;
long sav_loc = 0;              /* Location to go back to */
struct sess_con *sav_con = (struct sess_con *) NULL;
/****************************************************************************
 *    Initialise
 */
    pg.curr_event = (struct event_con *) NULL;
    pg.abort_event = (struct event_con *) NULL;
    pg.see_through=0;
    pg.esc_comm_flag=0;
    pg.log_output = stdout;
    pg.frag_size = 65536;
    pg.cur_in_file = stdin;
    pg.seqX = 0;                              /* timestamp sequencer       */
    pg.dumpout_flag = 0;
    while ( ( ch = getopt ( argc, argv, "a:l:vdh" ) ) != EOF )
    {
        switch ( ch )
        {
        case 'd' :
            pg.dumpout_flag=1;
            break;
        case 'a' :
            i = (atoi(strtok(optarg,",")));
            if ((x = strtok(NULL,",")) != NULL)
                j = atoi(x);
            else
                j = i;
            if (i > 0 && j > 0)
                set_def_binds(i,j);
            else
                fprintf(stderr,
"Ignoring attempt to set bind array size to %d, select array size to %d.\n\
Array sizes must be positive integers\n", i, j);
            break;
        case 'l' :
            set_long(atoi(optarg));
            break;
        case 'v' :
            pg.dumpout_flag=-1;
            break;
        case 'h' :
            (void) printf("sqldrive: SQL benchmark driver\n\
  You can specify:\n\
  -a to set array sizes (bind,select)\n\
  -l to set output buffer size\n\
  -d to see returned data\n\
  -v to see returned data and the resource costs\n\
 and -h to get this message.\n\
\n");
                    exit(0);
                default:
                case '?' : /* Default - invalid opt.*/
                       (void) fprintf(stderr,"Invalid argument; try -h\n");
                       exit(1);
                    break;
        }
    }
    if (argc - optind < 5)
    {
        fprintf(stderr,"Insufficient Arguments Supplied\n");
        exit(1);
    } 
    pg.logfile=argv[optind];
    pg.fdriver_seq=argv[optind + 1];            /* Details needed by event   */
    pg.bundle_seq=argv[optind + 2];             /* recording                 */
    pg.rope_seq=argv[optind + 3]; 
#ifdef INGRES
    all_dyns = hash(256, dynhash, dyncomp);
#endif

    event_record("S", (struct event_con *) NULL); /* announce the start */
    sav_id = strdup(argv[optind + 4]);
/*
 * Set up the hash table for events
 */
    pg.poss_events = hash(MAX_EVENT,long_hh,icomp);
/*
 * Initialise the input file processing, so that the session set-up time
 * is included in the first timing, and so that it is possible to spawn a
 * a sub-process before the database connection is made.
 */
    tbuf = malloc(WORKSPACE);
    tlook = malloc(WORKSPACE);
    look_status = CLEAR;
    tok_id = get_tok(stdin);
    if (tok_id == COMMAND && *tbuf == 'S')
    {
        start_timer(tbuf);
        tok_id = get_tok(stdin);
    }
/*
 * Spawn arbitrary startup command, if there is one.
 */
    if ( (start_event = getenv("PATH_STARTUP")) != (char *) NULL)
        system(start_event);
#ifndef INGRES
/*
 * Now initialise the active session
 */
    sqldrive_init(argv[optind + 4]);
#endif
#ifdef DEBUG
    fputs( "Init finished\n", stderr);
    fflush(stderr);
#endif

#ifndef SYBASE
#ifndef INGRES
    if (pg.dumpout_flag == -1)
        setup_costs(con);
#endif
#endif
#ifdef AIX
#ifdef SIGDANGER
    (void) sigset(SIGDANGER,SIG_IGN);
#endif
#endif
    (void) sigset(SIGUSR2,siggoaway);
                            /* Initialise the termination signal catcher */
#ifndef V32
#ifndef MINGW32
     (void) sigset(SIGTTOU,SIG_IGN);
                             /* Ignore silly stops */
     (void) sigset(SIGTTIN,SIG_IGN);
                             /* Ignore silly stops */
     (void) sigset(SIGTSTP,SIG_IGN);
                             /* Ignore silly stops */
#endif
#endif
     (void) sigset(SIGHUP,siggoaway);
                             /* Treat hangups as instructions to go away */
/*******************************************************************
 * Variables used to control main loop processing
 */
    pg.think_time = PATH_THINK;           /* default think time */
    saved_time = timestamp();
/*******************************************************************
 *  Main Loop;
 *  - feed information from stdin to ORACLE
 *
 *  Terminate when signal arrives (termination request SIGUSR2 or child death)
 *
 *  Process the file of statements and data with a simple parser
 */
    while (tok_id != PEOF)
    {
#ifdef DEBUG
        fflush(stdout);
        fflush(stderr);
#endif
        switch(tok_id)
        {
        case COMMAND:
#ifdef DEBUG
             fputs( "Seen COMMAND\n", stderr);
             fputs( tbuf, stderr);
             fputc( '\n', stderr);
             fflush(stderr);
#endif
            if (tbuf[0] == 'E')
            {
                tok_id = sqldrive_exec(tbuf);
#ifndef SYBASE
#ifndef INGRES
                if (pg.dumpout_flag == -1)
                    do_costs(con);
#endif
#endif
            }
            else
            {
                switch(tbuf[0])
                {
/*
 * Start an embedded sub-session
 */
                case 'L':
                    sav_con = con;
                    id = strdup(sav_id);
                    if ((con = dyn_connect(id, "sqldrive"))
                                       == (struct sess_con *) NULL)
                        con = sav_con;
                    free(id);
                    break;
/*
 * Terminate an embedded sub-session
 */
                case 'X':
#ifndef INGRES
                    sqldrive_quit();
                    con = sav_con;
#endif
                    break;
/*
 * Mark the loop location (for scripts that poll)
 */
                case 'M':
                    sav_loc = ftell(stdin);
                    if (pg.dumpout_flag)
                        printf("Mark seen at %u\n", sav_loc);
                    break;
/*
 * Return to the loop location (for scripts that poll)
 */
                case 'B':
                    look_status = CLEAR;
                    (void) fseek(stdin, sav_loc, 0);
                    if (pg.dumpout_flag)
                        printf("Looping back to %u\n", sav_loc);
                    break;
                case 'S':
                    start_timer(tbuf);
                    break;
                case 'T':
                    take_timer(tbuf);
                    break;
                case 'W':
                    take_timeout(tbuf);
                    break;
                case 'P':
                    sqldrive_parse(tbuf);
#ifndef SYBASE
#ifndef INGRES
                    if (pg.dumpout_flag == -1)
                        do_costs(con);
#endif
#endif
                    break;
                case 'I':
#ifdef INGRES
                    sqldrive_ingres(tbuf);
#endif
                    break;
                case 'R':
                    sqldrive_release(tbuf);
                    break;
                case 'F':
                    sqldrive_fetch(tbuf);
#ifndef SYBASE
#ifndef INGRES
                    if (pg.dumpout_flag == -1)
                        do_costs(con);
#endif
#endif
                    break;
                }
                tok_id = get_tok(stdin);
            }
            break;
        case SQL:
#ifdef DEBUG
             fputs( "Seen SQL\n", stderr);
             fputs( tbuf, stderr);
             fputc( '\n', stderr);
             fflush(stderr);
#endif
            tok_id = sqldrive_process(dyn);
#ifndef SYBASE
#ifndef INGRES
            if (pg.dumpout_flag == -1)
                do_costs(con);
#endif
#endif
            break;
        default:
            scarper(__FILE__, __LINE__,  "Syntax error in input stream");
            fprintf(stderr, "TOKEN: %d   Value: %s\n",tok_id,tbuf);
            tok_id = get_tok(stdin);
        }
    }
    free(tbuf);
    free(tlook);
    (void) fflush(stderr);
    (void) fflush(stdout);
    event_record("F", (struct event_con *) NULL);
    sqldrive_quit();
    exit(0);                                 /* scarper */
}    /* End of Main program */
/***********************************************************************
 * Connect to the database
 */
void sqldrive_init(arg1)
char      *arg1;
{
int i;
char * sp, * sp1;

    if ((con = dyn_connect(arg1, "sqldrive")) == (struct sess_con *) NULL)
    {
        scarper(__FILE__,__LINE__,"Failed to connect to database");
        exit(1);            /* Make certain we do not continue */
    }
    if (pg.dumpout_flag)
        puts("Connected to database");
    for (i = 0; i < sess_cnt; i++)
        if (all_sess[i] == NULL)
        {
            all_sess[i] = con;
            break;
        }
    if (i == sess_cnt)
    {
        all_sess[sess_cnt] = con;
        sess_cnt++;
    }
/*
 * Allocate the structure for unspecified cursors in advance
 */
    if ((dyn = dyn_init(con, NONSP_CURS)) == (struct dyn_con *) NULL)
        scarper(__FILE__,__LINE__,"Control Structure Allocation Failed");
/*
 * If there are global set statements defined, execute them
 */
    if ((sp = getenv("ING_SET")) != (char *) NULL)
    {
        sp = strdup(sp);
        for (sp1 = strtok(sp, ";");
                sp1 != (char *) NULL;
                     sp1 = strtok(NULL, ";"))
        {
            dyn->statement = sp1;
            prep_dml(dyn);
            exec_dml(dyn);
        }
        free(sp);
        dyn->statement = (char *) NULL;
    }
    return;
}
/**************************************************************************
 * Process a statement in the single cursor case.
 */
#ifdef INGRES
static void hexset(xp, xhp, xlp)
unsigned char * xp;
unsigned char * xhp;
unsigned char * xlp;
{
register unsigned char x;

    x = *xhp - (char) 48;
    if (x > (char) 48)
       x -= (char) 32;    /* Handle lower case */
    if (x > (char) 9)
       x -= (char) 7; 
    *xp = (unsigned char) (((int ) x) << 4);
    x = *xlp - (char) 48;
    if (x > (char) 48)
        x -= (char) 32;    /* Handle lower case */
    if (x > (char) 9)
        x -= (char) 7; 
    *xp |= x;
    return;
}
static void add_bv(d)
struct dyn_con * d;
{
int blen;

    if (d->head_bv == (struct bv *) NULL)
    {
        d->head_bv = (struct bv *) malloc(sizeof(struct bv));
        d->tail_bv = d->head_bv;
    }
    else
    {
        d->tail_bv->next =  (struct bv *) malloc(sizeof(struct bv));
        d->tail_bv  = d->tail_bv->next;
    }
    dyn_note(d, (char *) (d->tail_bv));
    d->tail_bv->next = (struct bv *) NULL;
    return;
}
/*
 * With INGRES, we get a buffer of details, for which the SQL statement
 * will be the last piece (except for the definition of a repeat query). There
 * may be all kinds of ancilliary details, particularly bind variables. 
 *
 * We arrange that the SQL is the last piece in the repeat query case by
 * dealing with it before returning control to the calling routine.
 */
static int deal_with_msgs(dynp, xp)
struct dyn_con **dynp;
char ** xp;
{
enum statement_type { STMT_NORMAL, STMT_DEFINE, STMT_REPEAT };
enum statement_type statement_type;
struct dyn_con *d = *dynp;
struct dyn_con *d1;
unsigned char * x = *xp;
unsigned char * top = x + strlen(x);
char *x1;
struct bv * this_bv;
struct dyn_con dh;
HIPT * h;
int i;
/**************************************************************************
 * Samples
 **************************************************************************
\I_PORT:1235\
M_SQL|2|
M_API_INT_TYPE|0|4|016E0000
M_API_INT_TYPE|0|4|67050000
M_API_CHA_TYPE|0|64|u_cl
M_SQL_FRAG|0|1023|
M_SQL_FRAG|0|1023|
M_SQL_FRAG|0|1023|
M_SQL_FRAG|0|1023|
M_SQL_FRAG|0|1023|
M_API_INT_TYPE|0|4|DC779C00
M_SQL_FRAG|0|49|define query  ~Q  is select division_id as
division_id, ... 
 .
 .
ld_addr4 as old_addr4, old_post_code as old_post_code, archived_ind as
archived_ind from hbclaim where claim_id= $0= ~V
M_EXEC_REPEAT|183|25|u_cl                                                            |

/
\I_PORT:1235\
M_EXEC_REPEAT|183|25|u_cl                                                            |
M_API_INT_TYPE|0|4|DC779C00

/
\I_PORT:1235\
M_PORT|1235|M_EXEC_REPEAT|183|9|ct5050                                                          |
M_API_INT_TYPE|0|4|DC779C00

/
\I_PORT:1235\
M_PORT|1235|M_PARSE_EXEC_FETCH|
M_SQL|17|
M_API_INT_TYPE|0|4|DC779C00
M_SQL_FRAG|0|363|select totals_year as totals_year, acct_debit as debit, acct_transitional as transitional, acct_write_off as write_off, acct_costs as costs, acct_penalties as penalties, acct_lump_disc as lump_disc, acct_refunds as refunds, acct_benefit as benefit, acct_remit as remit, acct_balance as balance from ctvacct_totals where account_ref= ~V  order by totals_year desc
/*************************************************************************
 * Dis-embodied bits and pieces
 *
M_API_MNY_TYPE|0|8|0F402EC400000000|0|
M_API_DTE_TYPE|0|12|1D00D5070400010000000000
*/
    statement_type = STMT_NORMAL;
    x1 = nextfield(x, '|'); 
    while (x1 != (char *) NULL && x < top)
    {
        x += strlen(x1) + 1;
        if (!strcmp(x1, "M_LOGIN"))
        {
/*
 * Launch a session
 */
            char * id;
            for (i = 0; i < sess_cnt; i++)
                if (all_sess[i] != NULL && all_sess[i]->port == cur_port)
                {
                    con = all_sess[i];
                    sprintf(dh.hash_key,"%d",cur_port);
                    if ((h = lookup(all_dyns, (char *) (& dh)))
                                == (HIPT *) NULL)
                    {   /* This happens if the log out failed ... */
                        if ((dyn = dyn_init(con, NONSP_CURS))
                              == (struct dyn_con *) NULL)
                            scarper(__FILE__,__LINE__,
                                      "Control Structure Allocation Failed");
                    }
                    else
                        dyn = (struct dyn_con *) (h->body);
                    break; 
                }
            if (i == sess_cnt)
            {
                id = strdup(sav_id);
                sqldrive_init(id);
                free(id);
            }
            if (con != (struct sess_con *) NULL
             && dyn != (struct dyn_con *) NULL)
            {
                con->port = cur_port;
                sprintf(dyn->hash_key,"%d",cur_port);
                insert(all_dyns, (char *) dyn, (char *) dyn);
            }
            return 0;
        }
        else
        if (!strcmp(x1, "M_DISCONNECT"))
        {
/*
 * Terminate a session
 */
            sqldrive_quit();
            return 0;
        }
        else
        if (!strcmp(x1, "M_TERMINATE"))
        {
/*
 * Terminate all sessions
 */
            sqldrive_quit_all();
            return 0;
        }
        else
        if (!strcmp(x1, "M_COMMIT"))
        {
/*
 * Commit
 */
            dbms_commit(con);
            return 0;
        }
        else
        if (!strcmp(x1, "M_DEFINE") || !strcmp(x1, "M_PARSE"))
        {
/*
 * Repeat Query Definition
 */
            statement_type = STMT_DEFINE;
        }
        else
        if (!strcmp(x1, "M_EXEC_REPEAT"))
        {
        unsigned char * kp;
/*
 * Repeat Query Execution
 */
            statement_type = STMT_REPEAT;
            kp = x;
            x1 = nextfield(NULL,'|');
            x += strlen(x1) + 1;
            x1 = nextfield(NULL,'|');
            x += strlen(x1) + 1;
            x1 = nextfield(NULL,'|');
            if (strlen(x1) > 64)
                x += 65;
            else
                x += strlen(x1) + 1;
            sprintf(dh.hash_key,"%d:%.*s",cur_port, (x - kp - 1), kp);
            if ((h = lookup(all_dyns, (char *) (& dh))) == (HIPT *) NULL)
            {
                fprintf(stderr, "Logic Error: repeat query %s not found\n",
                       dh.hash_key);
                iterate(all_dyns, 0, dyn_dump); /* List what is there  */
                return 0;                       /* Abandon processing  */
            }
            else
            {
                d1 = (struct dyn_con *) (h->body);
                dyn_forget(d1);
                d1->anchor = NULL;
                d1->head_bv = NULL;
                d1->tail_bv = NULL;
                d = d1;
                d->queryType = IIAPI_QT_EXEC_REPEAT_QUERY;
                *dynp = d;
            }
        }
        else
        if (!strcmp(x1, "M_PARSE_EXEC_FETCH"))
        {
/*
 * Normal Execution
 */
            statement_type = STMT_NORMAL;
        }
        if (!strcmp(x1, "M_SQL_FRAG"))
        {
/*
 * The SQL Text. We need to check if this is the last one. If it is, we
 * set things up and return.
 */
            x = strchr(x,'|') + 1;
            x = strchr(x,'|') + 1;
            if (*x != '\n' && strncmp(x, "M_PARSE", 7))
            {
/*
 * This is the statement to be executed.
 */
                *xp = x;
                if (d == (struct dyn_con *) NULL)
                {
                    char * id;
                    fprintf(stderr,
                        "Logic Error: NULL dyn_con for port %d\n%s\n",
                              cur_port, x);
                    id = strdup(sav_id);
                    sqldrive_init(id);
                    free(id);
                    d = dyn;
                }
                if (statement_type == STMT_DEFINE)
                {
                char * kp;
/*
 * The SQL statement has the magic numbers for the repeat statement
 * tacked on the end. Find the final '|', find the line feed (that actually
 * marks the end of the SQL. Then use the the pair of numbers and the name
 * as the key.
 */

                    x1 = nextfield(x, '|');
                    kp = x + strlen(x1) - 14;
                    *kp = '\0';
                    kp++;
                    x1 = nextfield(kp, '|');
                    if (x1 == (char *) NULL || strcmp(x1,"M_EXEC_REPEAT"))
                    {
                        fprintf(stderr,
                             "Logic Error: define followed by %s\n",
                                  kp);
                        if (x1 != (char *) NULL)
                            fprintf(stderr,"x1 = %s\n", x1);
                        return 0;
                    }
                    kp += strlen(x1) + 1;
                    if (((x1 = strchr(kp,'\n')) == (char *) NULL)
                      ||((x = strchr(kp,'|')) == (char *) NULL)
                      ||((x = strchr(x + 1,'|')) == (char *) NULL)
                      ||(((x + 65) > x1)))
                    {
                        fprintf(stderr,"Logic Error: define followed by %s\n",
                                  kp);
                        return 0;
                    }
                    *(x + 65) = '\0';
                    sprintf(dh.hash_key, "%d:%s",cur_port, kp);
/*
 * Kill it off if we already have one
 */
                    if ((h = lookup(all_dyns, (char *) (& dh)))
                                 != (HIPT *) NULL)
                        dyn_kill((struct dyn_con *) (h->body));
/*
 * Create a new one. We don't use the cursor number with INGRES, but a 
 * value is needed to make the ORACLE and Sybase compatible code work.
 */
                    d1 = dyn_init(con, con->curs_no++);
                    sprintf(d1->hash_key,"%d:%s",cur_port, kp);
                    insert(all_dyns, (char *) d1, (char *) d1);
/*
 * Transfer the bind variable chain over to the control structure
 */
                    d1->anchor = d->anchor;
                    d->anchor = NULL;
                    d1->head_bv = d->head_bv;
                    d->head_bv = NULL;
                    d1->tail_bv = d->tail_bv;
                    d->tail_bv = NULL;
                    *dynp = d1;         /* Plug in the new control structure */
                    d1->queryType =  IIAPI_QT_DEF_REPEAT_QUERY;
                }
                return 1;
            }
        }
        else
        if (!strcmp(x1, "M_ROLLBACK"))
        {
/*
 * Rollback
 */
            dbms_roll(con);
            return 0;
        }
        else
        if (!strcmp(x1, "M_CANCEL"))
        {
/*
 * Cancel - meaningless at the moment, because we fetch everything.
 */
            return 0;
        }
        else
        if (!strncmp(x1, "M_API_", 6))
        {
        short int vlen;
        double f;
/*
 * A Bind variable
 */
            add_bv(d);
            x1 += 6;
            this_bv = d->tail_bv;
            if (!strcmp(x1,"DTE_TYPE"))
            {
                this_bv->dbtype = 0x3;
            }
            else
            if (!strcmp(x1,"MNY_TYPE"))
            {
                this_bv->dbtype = 0x5;
            }
            else
            if (!strcmp(x1,"DEC_TYPE"))
            {
                this_bv->dbtype = 0xa;
            }
            else
            if (!strcmp(x1,"LOGKEY_TYPE"))
            {
                this_bv->dbtype = 0xb;
            }
            else
            if (!strcmp(x1,"TABKEY_TYPE"))
            {
                this_bv->dbtype = 0xc;
            }
            else
            if (!strcmp(x1,"CHA_TYPE"))
            {
                this_bv->dbtype = 0x14;
            }
            else
            if (!strcmp(x1,"VCH_TYPE"))
            {
                this_bv->dbtype = 0x15;
            }
            else
            if (!strcmp(x1,"LVCH_TYPE"))
            {
                this_bv->dbtype = 0x16;
            }
            else
            if (!strcmp(x1,"BYTE_TYPE"))
            {
                this_bv->dbtype = 0x17;
            }
            else
            if (!strcmp(x1,"VBYTE_TYPE"))
            {
                this_bv->dbtype = 0x18;
            }
            else
            if (!strcmp(x1,"LBYTE_TYPE"))
            {
                this_bv->dbtype = 0x19;
            }
            else
            if (!strcmp(x1,"NCHA_TYPE"))
            {
                this_bv->dbtype = 0x1a;
            }
            else
            if (!strcmp(x1,"NVCH_TYPE"))
            {
                this_bv->dbtype = 0x1b;
            }
            else
            if (!strcmp(x1,"LNVCH_TYPE"))
            {
                this_bv->dbtype = 0x1c;
            }
            else
            if (!strcmp(x1,"INT_TYPE"))
            {
                this_bv->dbtype = 0x1e;
            }
            else
            if (!strcmp(x1,"FLT_TYPE"))
            {
                this_bv->dbtype = 0x1f;
            }
            else
            if (!strcmp(x1,"CHR_TYPE"))
            {
                this_bv->dbtype = 0x20;
            }
            else
            if (!strcmp(x1,"TXT_TYPE"))
            {
                this_bv->dbtype = 0x25;
            }
            else
            if (!strcmp(x1,"LTXT_TYPE"))
            {
                this_bv->dbtype = 0x29;
            }
            else
            if (!strcmp(x1,"HNDL_TYPE"))
            {
                this_bv->dbtype = -1;
            }
            x1 = nextfield(NULL, '|');
            x += strlen(x1) + 1;
            x1 = nextfield(NULL, '|');
            x += strlen(x1) + 1;
            this_bv->nullok = 1;
            this_bv->prec = 0;
            this_bv->scale = 0;
            this_bv->bname = " ~V ";
            this_bv->blen = 4;
            this_bv->dbsize = atoi(x1);
            if (this_bv->dbsize > 2000)
            {
                fprintf(stderr,"Dodgy length: %d in %s\n",
                        this_bv->dbsize, x); 
                this_bv->dbsize = 2000;
                return 0;
            }
            this_bv->dsize = this_bv->dbsize ;
            this_bv->val  = (char *) malloc(this_bv->dsize + 3);
            dyn_note(d, this_bv->val);
            switch (this_bv->dbtype)
            {
            case 0x5: /* MNY_TYPE */
                memset(this_bv->val, 0, this_bv->dsize);
                hexset(this_bv->val + 7, x + 2, x);
                hexset(this_bv->val + 6, x + 1, x + 6);
                hexset(this_bv->val + 5, x + 7, x + 4);
                hexset(this_bv->val + 4, x + 5, x + 10);
                this_bv->vlen = this_bv->dsize;
                break;
            case 0xa: /* DEC_TYPE */
                (void) hexout(this_bv->val, x, this_bv->dsize);
                this_bv->vlen = this_bv->dsize;
                break;
            case 0x1f: /* FLT_TYPE */
                f = strtod(x, &x);
                memcpy( this_bv->val, (char *) &f, sizeof(double));
                this_bv->vlen = this_bv->dsize;
                break;
            case 0x14: /* CHA_TYPE */
            case 0x20: /* CHR_TYPE */
            case 0x1a: /* NCHA_TYPE */
            case 0x25: /* TXT_TYPE */
            case 0x29: /* LTXT_TYPE */
                memcpy( this_bv->val, x, this_bv->dsize);
                this_bv->val[this_bv->dsize] = '\0';
                x += this_bv->dsize;
                this_bv->vlen = this_bv->dsize;
                break;
            case 0x3: /* DTE_TYPE */
            case -1: /* HNDL_TYPE */
            case 0xb: /* LOGKEY_TYPE */
            case 0xc: /* TABKEY_TYPE */
            case 0x1e: /* INT_TYPE */
            case 0x17: /* BYTE_TYPE */
            case 0x19: /* LBYTE_TYPE */
                (void) hexout(this_bv->val, x, this_bv->dsize);
                this_bv->vlen = this_bv->dsize;
                x += 2 * this_bv->dsize;
                break;
            case 0x15: /* VCH_TYPE */
            case 0x16: /* LVCH_TYPE */
            case 0x1b: /* NVCH_TYPE */
            case 0x1c: /* LNVCH_TYPE */
                x1 = nextfield(NULL, '|');
                this_bv->vlen = atoi(x1);
                vlen = (short int) this_bv->vlen;
                x += strlen(x1) + 1;
                memcpy( this_bv->val, (char *) &vlen, 2);
                memcpy( this_bv->val + 2, x, this_bv->vlen);
                this_bv->val[this_bv->vlen + 2] = '\0';
                x += this_bv->vlen;
                this_bv->vlen = this_bv->dsize;
                break;
            case 0x18: /* VBYTE_TYPE */
                x = nextfield(NULL, '|');
                this_bv->vlen = atoi(x1);
                x += strlen(x1) + 1;
                (void) hexout(this_bv->val, x, this_bv->vlen);
                x += 2 * this_bv->vlen;
                break;
            }
        }
/*
 * Anything else we just skip to the next line
 */
        if (x >= top )
        {
            *xp = top;
            return 1;
        }
        if (*x == '\0')
        {
            *xp = x;
            return 1;
        }
        while(x < top && *x != '\n')
            x++;
        x++;
        if (x >= top )
        {
            *xp = top;
            return 1;
        }
        if (*x == '\0')
        {
            *xp = x;
            return 1;
        }
        x1 = nextfield(x, '|');
    }
/*
 * Not sure if this can happen, but try to make it do something sensible
 */
    for (x = *xp; x < top && *x != '\0'; x++);
        *xp = x;
    return 1;
}
/******************************************************************
 * INGRES - Switch the session if necessary.
 */
static void sqldrive_ingres(cur_pos)
char * cur_pos;
{
HIPT * h;
struct dyn_con d;
char *x;
int curs_no;
enum tok_id tok_id;

    (void) nextfield(cur_pos, ':');
    if ((x =  nextfield(NULL, ':')) == (char *) NULL)
        return;
    strcpy(d.hash_key, x);
    cur_port = atoi(x);
    if ((h = lookup(all_dyns, (char *) (& d))) == (HIPT *) NULL)
    {
        fprintf(stderr, "No session for port %d\n", cur_port);
        dyn = (struct dyn_con *) NULL;
        con = (struct sess_con *) NULL;
        return;
    }
    dyn = (struct dyn_con *) (h->body);
    con = dyn->con;
#ifdef DEBUG_FULL
    fputs("Con connection\n", stderr);
    iterate(con->ht, 0, dyn_dump); /* List what is there  */
    fputs("All statements connection\n", stderr);
    iterate(all_dyns, 0, dyn_dump); /* List what is there  */
#endif
    return;
}
#endif
static enum tok_id sqldrive_process(d)
struct dyn_con *d;
{
enum tok_id tok_id;
char * x = tbuf;

    while (*x == ' ' || *x == '\n')
        x++;
    if (*x == '\0')
        return get_tok(stdin);   /* Do nothing if there is nothing to do ... */
#ifdef INGRES
    if (d != NULL)
        d->queryType = 0;
    if (*x == 'M' && *(x + 1) == '_')
        if (!deal_with_msgs(&d, &x))
            return get_tok(stdin);
    if (*x == '\0'
      && (d == (struct dyn_con *) NULL
        || d->queryType != IIAPI_QT_EXEC_REPEAT_QUERY))
    {
        fprintf(stderr, "NO-OP? --->\n%s\n=======\n", tbuf);
        return get_tok(stdin);
    }
#endif
    if (con == NULL)
    {
        char * id = strdup(sav_id);
        sqldrive_init(id);
        free(id);
    }
    if (d == NULL)
        d = dyn;
    while(isspace(*x))
        x++;
    d->statement = strdup(x);
    if (pg.dumpout_flag)
    {
#ifdef DEBUG
        fputs("All statements\n", stderr);
        iterate(all_dyns, 0, dyn_dump); /* List what is there  */
        fputs("This connection statement\n", stderr);
        iterate(con->ht, 0, dyn_dump); /* List what is there  */
        fflush(stderr);
#endif
        puts("PARSE:");
        puts(tbuf);
        fflush(stdout);
    }
    prep_dml(d);
    if (pg.dumpout_flag)
        puts("EXEC:");
    tok_id = dyn_exec(stdin,d);
    if (d->bdp != (E2SQLDA *) NULL && pg.dumpout_flag)
    {
        fflush(stderr);
        bind_print(stdout,d);
        fflush(stdout);
    }
    if (pg.dumpout_flag)
    {
        puts("FETCH:");
        res_process(pg.log_output,(FILE *) NULL,d, form_print,1);
        fflush(stderr);
        fflush(stdout);
    }
    else
        res_process(stderr,(FILE *) NULL,d, NULL,1);
    free(d->statement);
    d->statement = (char *) NULL; 
    return tok_id;
}
#ifdef INGRES
void zap_stat_hash(struct dyn_con * d)
{
    hremove(all_dyns, (char *) d);
    return;
}
#endif
/************************************************************************
 * Closedown
 */
void sqldrive_quit()
{
int i;

    if (con != (struct sess_con *) NULL)
    {
        dbms_commit(con);
        if (dyn_disconnect(con))
        {
            if (pg.dumpout_flag)
                puts("Disconnected from database");
            for (i = 0; i < sess_cnt; i++)
                if (all_sess[i] == con)
                {
                    all_sess[i] = NULL;
                    break;
                }
            con = NULL;
        }
        else
        {
            puts("Disconnect failed");
            fputs("After attempted disconnect - for connection\n", stderr);
            iterate(con->ht, 0, dyn_dump); /* List what is there  */
#ifdef INGRES
            fputs(" - all statements connection\n", stderr);
            iterate(all_dyns, 0, dyn_dump); /* List what is there  */
#endif
        }
        dyn = NULL;
    }
    return;
}
void sqldrive_quit_all()
{
int i;

    for (i = 0; i < sess_cnt; i++)
    {
        con = all_sess[i];
        if (con != NULL)
            sqldrive_quit();
    }
#ifdef INGRES
    cleanup(all_dyns);
    all_dyns = hash(256, dynhash, dyncomp);
#endif
    sess_cnt = 0;
    return;
}
/******************************************************************
 * Start taking timings
 */
static void start_timer(cur_pos)
char * cur_pos;
{
HIPT * h;
short int l;
short int x;

    l=0;
    cur_pos++;
    stamp_declare(cur_pos);
    x = (((int) (*cur_pos)) << 8) + ((int) *(cur_pos+1));
    if ((h = lookup(pg.poss_events,(char *) ((long) x))) == (HIPT *) NULL)
    {
        (void) fprintf(stderr,"Error, event define failed for %s\n",
                      cur_pos);
        return;
    }
    pg.curr_event = (struct event_con *) (h->body);
    pg.curr_event->time_int = timestamp();
    return;
}
/*******************************************************************
 * Take a time stamp.
 */
static void take_timer(cur_pos)
char * cur_pos;
{
HIPT * h;
short int l;
short int x;

    l=0;
/*    sort_out(dyn->con); */
    cur_pos++;
    cur_pos = nextfield(cur_pos,':');
    x = (((int) (*cur_pos)) << 8) + ((int) *(cur_pos+1));
    if ((h = lookup(pg.poss_events, ((char *) ((long) x)))) ==
          (HIPT *) NULL)
    {
        (void) fprintf(stderr,"Error, undefined event %*.*s\n",
                      sizeof(x),sizeof(x),
                      (char *) &x);
        return;
    }
    pg.curr_event = (struct event_con *) (h->body);
    if (pg.curr_event  != (struct event_con *) NULL)
    {
        int think_left;
                      /* record the time */
        pg.curr_event->word_found = traff_log(dyn); 
        pg.force_flag = FORCE_DUE;  /* Switch on statistics logging */
        event_record(pg.curr_event->event_id, pg.curr_event);
        pg.force_flag = 0;    /* Switch it off again */
        think_left = (int) (pg.curr_event->min_delay
                    - pg.curr_event->time_int/100.0);
        while (think_left > 0)
        {                       /* sleep time in progress */
            think_left = sleep(think_left);
        }
        pg.curr_event = (struct event_con *) NULL;
    }
    return;
}
/*******************************************************************
 * Sleep for some number of seconds
 */
static void take_timeout(cur_pos)
char * cur_pos;
{
    int thinks;
    cur_pos++;
    thinks = atoi(cur_pos);
    if (thinks > 0)
        sleep(thinks);
    pg.think_time = thinks;
    return;
}
/******************************************************************
 * Parse a statement.
 */
static void sqldrive_parse(cur_pos)
char * cur_pos;
{
struct dyn_con * d;
char * x;
int curs_no;
enum tok_id tok_id;

    (void) nextfield(cur_pos, ':');
    if ((x =  nextfield(NULL, ':')) == (char *) NULL)
        return;
    curs_no = atoi(x);
    d = dyn_init(con,curs_no);
    tok_id = get_tok(stdin);
    if (tok_id != SQL)
    {
        (void) fprintf(stderr,"No statement found to parse\n");
        return;
    }
    d->statement = strdup(tbuf);
    if (pg.dumpout_flag)
    {
        printf("PARSE:%d\n",curs_no);
        puts(tbuf);
    }
    prep_dml(d);
    return;
}
/******************************************************************
 * Release a cursor
 */
static void sqldrive_release(cur_pos)
char * cur_pos;
{
struct dyn_con * d;
char * x;
int curs_no;

    (void) nextfield(cur_pos, ':');
    if ((x =  nextfield(NULL, ':')) == (char *) NULL)
        return;
    curs_no = atoi(x);
    if ((d = dyn_find(con, curs_no)) != (struct dyn_con *) NULL)
        (void) dyn_kill(d);
    else
        scarper(__FILE__, __LINE__, "Failed to release cursor:%d", curs_no);
    return;
}
/******************************************************************
 * Copy values from descriptors into variables
 *
 * The arguments are:
 * - The statement block (needed for ROWID)
 * - The appropriate descriptor
 * A multiply occurring group consisting of:
 * - Row (numbered from 1 to n)
 * - Column (numbered from 1 to n)
 * - Variable name to use for re-use
 */
static void sqldrive_read(d,dp)
struct dyn_con *d;
E2SQLDA *dp;
{
char * x;
struct symbol * sym;
int row;
int col;

    if ((x =  nextfield(NULL, ':')) == (char *) NULL)
        return;
    do
    {
    register char *y;

        row = atoi(x) - 1;
        if  ((x = nextfield(NULL, ':')) == (char *) NULL)
            break;
        col = atoi(x) -1;
        if  ((x = nextfield(NULL, ':')) == (char *) NULL)
            break;
        if (dp == (E2SQLDA * ) NULL
             || col < -1 || row < -1
             || row >= dp->arr
             || col >= dp->N)
           continue;
        for (y = x; *y != '\0'; y++)
            if (islower(*y))
                *y = _toupper(*y);
        sym = newsym(x, STOK); 
        if (sym->sym_value.tok_ptr != (char *) NULL
         && sym->sym_value.tok_len > 0)
        {
            free(sym->sym_value.tok_ptr);
            sym->sym_value.tok_ptr = (char *) NULL;
            sym->sym_value.tok_len = 0;
        }
        if (d->so_far == 0 || d->so_far <= row)
        {   /* If no row returned, make it NULL */
            sym->sym_value.tok_ptr = strdup("");
            sym->sym_value.tok_len = 0;
        }
/*
 * Get the ROWID
 */
        if (col < 0 || row< 0)
        {
#ifdef SYBASE
            sym->sym_value.tok_ptr = strdup("");
            sym->sym_value.tok_len = 0;
#else
#ifdef INGRES
            sym->sym_value.tok_ptr = strdup("");
            sym->sym_value.tok_len = 0;
#else
            sym->sym_value.tok_ptr = strdup(dbms_rowid(d));
            sym->sym_value.tok_len = strlen(sym->sym_value.tok_ptr);
#endif
#endif
        }
        else
        {
        char buf[256];

            sym->sym_value.tok_len =
                   save_value(&(sym->sym_value.tok_ptr),dp,row,col);
            switch ((int) dp->T[col])
            {
            int i;
            unsigned long u;

#ifndef SYBASE
#ifndef INGRES
            case ORA_DATE:
                ordate( buf, sym->sym_value.tok_ptr);
                (void) date_val(buf,"dd Mon yyyy hh24:mi:ss",
                             &x,&(sym->sym_value.tok_val));
                sym->sym_value.tok_type = DTOK;
                break;
            case ORA_NUMBER:
                (void) ora_num( sym->sym_value.tok_ptr,buf,
                                  sym->sym_value.tok_len);
                sym->sym_value.tok_val = strtod(buf,(char **) NULL);
                sym->sym_value.tok_type = NTOK;
                break;
#endif
#endif
            case ORA_FLOAT:
                memcpy( (char *) &(sym->sym_value.tok_val),
                       sym->sym_value.tok_ptr, sizeof(double));
                sym->sym_value.tok_len  = sprintf( buf, "%.13g", 
                                                    sym->sym_value.tok_val);
                sym->sym_value.tok_type = NTOK;
                free( sym->sym_value.tok_ptr);
                sym->sym_value.tok_ptr = strdup(buf);
                break;
            case ORA_INTEGER:
                memcpy( (char *) &i,  sym->sym_value.tok_ptr, sizeof(int));
                sym->sym_value.tok_len  = sprintf( buf, "%d", i);
                sym->sym_value.tok_val  = (double) i;
                sym->sym_value.tok_type = NTOK;
                free( sym->sym_value.tok_ptr);
                sym->sym_value.tok_ptr = strdup(buf);
                break;
#ifndef INGRES
            case ORA_UNSIGNED:
                memcpy( (char *) &u,  sym->sym_value.tok_ptr,
                               sizeof(unsigned long));
                sym->sym_value.tok_len  = sprintf( buf, "%u", u);
                sym->sym_value.tok_val  = (double) u;
                sym->sym_value.tok_type = NTOK;
                free( sym->sym_value.tok_ptr);
                sym->sym_value.tok_ptr = strdup(buf);
                break;
#endif
            default:
                sym->sym_value.tok_val = strtod(sym->sym_value.tok_ptr,&x);
#ifndef EQUANT
                if (sym->sym_value.tok_val != 0.0
                 && ((x - sym->sym_value.tok_ptr) >= sym->sym_value.tok_len))
                    sym->sym_value.tok_type = NTOK;
#endif
                break;
            }
            sym->sym_value.tok_conf = INUSE; /* Ensure it is marked in use */
        }
        if (pg.dumpout_flag)
        {
            printf("Saved Returned Value\n");
            dumpsym(sym);
        }
    }
    while ((x =  nextfield(NULL, ':')) != (char *) NULL);
    return;
}
/******************************************************************
 * Execute a statement. This includes the bind processing.
 */
static enum tok_id sqldrive_exec(cur_pos)
char * cur_pos;
{
struct dyn_con * d;
char * x;
int curs_no;
enum tok_id tok_id;
char buf[256];

    strcpy(&buf[0],cur_pos);
    (void) nextfield(cur_pos, ':');
    if ((x =  nextfield(NULL, ':')) == (char *) NULL)
        return get_tok(stdin);
    curs_no = atoi(x);
#ifdef EQUANT
    if (curs_no == 3)
    {
        (void) get_tok(stdin);
        do_desc(con, tbuf);
        (void) get_tok(stdin);
        return get_tok(stdin);
    }
#endif
    if ((d = dyn_find(con,curs_no)) == (struct dyn_con *) NULL)
    {
        (void) fprintf(stderr,"No statement found to execute\n");
        return get_tok(stdin);
    }
    else
    {
        if (pg.dumpout_flag)
            printf("EXEC:%d\n",curs_no);
        tok_id =  dyn_exec(stdin,d);
        (void) nextfield(&buf[0], ':');
        (void) nextfield(NULL, ':');
        if (d->bdp != (E2SQLDA *) NULL)
        {
            sqldrive_read(d, d->bdp);
            if ( pg.dumpout_flag)
                bind_print(stdout,d);
        }
        return tok_id;
    }
}
/******************************************************************
 * Fetch against a statement.
 */
static void sqldrive_fetch(cur_pos)
char * cur_pos;
{
struct dyn_con * d;
char * x;
int curs_no;

    (void) nextfield(cur_pos, ':');
    if ((x =  nextfield(NULL, ':')) == (char *) NULL)
        return;
    curs_no = atoi(x);
/*
 * Clear down the list of saved returned values
 */
    if ((d = dyn_find(con,curs_no)) == (struct dyn_con *) NULL)
    {
        (void) fprintf(stderr,"No statement found to fetch for:%d\n", curs_no);
        return;
    }
    if (pg.dumpout_flag)
    {
        puts("FETCH:");
        res_process(pg.log_output,(FILE *) NULL,d, form_print,0);
    }
    else
        res_process(stderr,(FILE *) NULL,d, NULL,0);
    sqldrive_read(d, d->sdp);
    return;
}
/**************************************************************************
 * Dummy functions to ensure that only timestamp.o is pulled in from the
 * pathatlib.a library
 */
void match_out (curr_word)
struct word_con * curr_word;
{
    if (curr_word != (struct word_con *) NULL)
        int_out(curr_word->words);
    return;
}
/**********************************************************************
 * Clear the statistics
 */
static void traf_clear(dyn)
struct dyn_con * dyn;
{
    if (dyn == (struct dyn_con *) NULL)
        return;
    dyn->chars_read = 0;
    dyn->chars_sent = 0;
    dyn->rows_read = 0;
    dyn->rows_sent = 0;
    dyn->fields_read = 0;
    dyn->fields_sent = 0;
    return;
} 
/**********************************************************************
 * Set up to log the traffic statistics with the events
 */
static struct word_con * traff_log(dyn)
struct dyn_con * dyn;
{
register char *x;
register short int * y;
static char buf[BUFLEN];
static short int ibuf[BUFLEN];
static struct word_con w;
w.words = &ibuf[0];
w.tail = &ibuf[0];
w.head = &ibuf[0];

    if (dyn == (struct dyn_con *) NULL)
        buf[0] = '\0';
    else
    sprintf(&buf[0],":%d:%d:%d:%d:%d:%d",
             dyn->chars_read,     /* Count of characters read by selects */
             dyn->chars_sent,     /* Length of SQL Statements            */
             dyn->rows_read,      /* Count of rows processed             */
             dyn->rows_sent,      /* Count of rows processed             */
             dyn->fields_read,    /* Count of fields read                */
             dyn->fields_sent);    /* Count of fields sent                */
    for (x = &buf[0], y = &ibuf[0]; *x != (char) 0; *y++ = (short int) *x++); 
    *y = 0;
    w.state = y;
    w.head = y;
    traf_clear(dyn);
    return &w;
} 
#ifndef SYBASE
#ifndef INGRES
/*******************************************************************
 * Resource cost logging
 *
 * Find the correct accumulator
 */
struct cost_accum * find_cost_accum(con,stat_len,stat_no)
struct sess_con * con;
int stat_len;
char * stat_no;
{
struct cost_accum * cp = con->costs;
int i = con->cost_cnt;

    while(i)
    {
        if (!strncmp(cp->stat_no,stat_no,stat_len)
           && *(cp->stat_no + stat_len) == '\0')
            break; 
       i--;
       cp++;
    }    
    if (i <= 0)
        return (struct cost_accum *) NULL;
    else
        return cp;
}
/*
 * Move the current value to the last value for the resource costs
 */
static void roll_costs(con)
struct sess_con * con;
{
int i = con->cost_cnt;
struct cost_accum * cp = con->costs;

    while(i)
    {
       cp->last_value = cp->cur_value;
       cp++;
       i--;
    }
    return;
}
/*
 * Print out the differential costs.
 */
static void print_costs(con)
struct sess_con * con;
{
int i = con->cost_cnt;
struct cost_accum * cp = con->costs;

    while(i)
    {
       if (cp->cur_value != cp->last_value)
           printf("%s:%.26g\n", cp->stat_name,(cp->cur_value - cp->last_value));
       cp++;
       i--;
    }
    return;
}
double strod();
static void do_costs(con)
struct sess_con * con;
{
    get_costs(con);
    print_costs(con);
    roll_costs(con);
    return;
}
/*
 * Read the accumulators from the ORACLE table
 */
static void get_costs(con)
struct sess_con * con;
{
struct cost_accum * cp = con->costs;
struct dyn_con * d;
int len[2];
char * f_ptr[2];

    if ((d = dyn_find(con,COST_CURS)) == (struct dyn_con *) NULL)
    {
        (void) fprintf(stderr,"No statement found to fetch\n");
        return;
    }
    exec_dml(d);
    desc_sel(d);
    while ( dyn_locate(d,&len[0],&f_ptr[0]))
    {
    char buf[26];
        if ((cp = find_cost_accum(con, len[0], f_ptr[0]))
                == (struct cost_accum *) NULL)
        {
            scarper(__FILE__,__LINE__,
                    "Failed to find resource record");
            abort();
        } 
        memcpy(&buf[0],f_ptr[1],len[1]);
        buf[len[1]] = '\0';
        cp->cur_value = strtod(&buf[0],(char **) NULL);
    }
    return;
}
/*
 * Setup the collection of statement costs
 */
static void setup_costs(con)
struct sess_con * con;
{
struct dyn_con * dyn;
struct cost_accum * cp;
char buf[256];
int len[2];
char * f_ptr[2];
static char * name_get = "select statistic#,name from v$statname \
where name in (\
'recursive cpu usage',\
'CPU used when call started',\
'CPU used by this session',\
'db block gets',\
'consistent gets',\
'physical reads',\
'physical writes',\
'table scans (short tables)',\
'table scans (long tables)',\
'table scan rows gotten',\
'table scan blocks gotten',\
'table fetch by rowid',\
'parse time cpu',\
'sorts (memory)',\
'sorts (disk)',\
'sorts (rows)')";
/*
 * Begin by identifying the items of interest from the statistics table
 */
    if ((dyn = dyn_init(con, COST_CURS)) == (struct dyn_con *) NULL)
        scarper(__FILE__,__LINE__,"Control Structure Allocation Failed");
    dyn->statement = name_get;
    prep_dml(dyn);
    exec_dml(dyn);
    desc_sel(dyn);
    for (con->cost_cnt = 0,
         con->costs = calloc(16,sizeof(struct cost_accum)),
         cp = con->costs;
             dyn_locate(dyn,&len[0],&f_ptr[0]);
                  cp++,
                  con->cost_cnt++)
    {
        memcpy(&buf[0],f_ptr[0],len[0]);
        buf[len[0]] = '\0';
        cp->stat_no = strdup(&buf[0]); /* The statistic number */
        memcpy(&buf[0],f_ptr[1],len[1]);
        buf[len[1]] = '\0';
        cp->stat_name = strdup(buf);  /* The statistic number */
    }
    if (con->cost_cnt < 16)
    {
        fputs("Cost information not accessible\n", stderr);
        pg.dumpout_flag = 1;   /* Switch off cost logging   */
        return;
    }
/*
 * Find the SID
 */
    sprintf(buf,"select sid from v$session where process = '%d'",getpid());
    dyn->statement = buf;
    prep_dml(dyn);
    exec_dml(dyn);
    desc_sel(dyn);
    dyn_locate(dyn,&len[0],&f_ptr[0]);
/*
 * Set up the Statement to gather timings
 */
    sprintf(buf,"select statistic#, value  from v$sesstat where sid = %*.*s \
and statistic# in (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s) \
and value > 0",
    len[0],len[0],f_ptr[0],
    con->costs[0].stat_no,
    con->costs[1].stat_no,
    con->costs[2].stat_no,
    con->costs[3].stat_no,
    con->costs[4].stat_no,
    con->costs[5].stat_no,
    con->costs[6].stat_no,
    con->costs[7].stat_no,
    con->costs[8].stat_no,
    con->costs[9].stat_no,
    con->costs[10].stat_no,
    con->costs[11].stat_no,
    con->costs[12].stat_no,
    con->costs[13].stat_no,
    con->costs[14].stat_no,
    con->costs[15].stat_no);
    prep_dml(dyn);
/*
 * Initialise ready for use
 */
    get_costs(con);
    roll_costs(con);
    return;
}
#endif
#endif
