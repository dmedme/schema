/************************************************************************
 * tabdiff.h - Header for tabdiff and related programs.
 *
@(#) $Name$ $Id$
*/
#ifndef TABDIFF_H
#define TABDIFF_H
#ifdef AIX
#define __inline
#endif
#ifdef ULTRIX
#define __inline
#endif
#ifdef HP7
#define __inline
#endif
#ifndef MINGW32
#include <sys/param.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include "natregex.h"
#include "hashlib.h"
#include "e2conv.h"
#include "e2dfflib.h"
#include "csexe.h"
#ifdef SQLITE3
#include <sqlite3.h>
#endif
#ifdef SYBASE
#include <ctpublic.h>
#endif
#ifdef INGRES
#include <iiapi.h>
#endif
/*****************************************************************
 * The data for processing the statement files
 */
#ifndef PATHSIZE
#ifndef MAXPATHLEN
#ifdef MINGW32
#define MAXPATHLEN MAX_PATH
#else
#define MAXPATHLEN 256
#endif
#endif
#define PATHSIZE MAXPATHLEN
#endif
#define WORKSPACE 65536
#define MAX_SCRAM 128

enum look_status {CLEAR, PRESENT};

enum tok_id {SQL, COMMAND, FIELD, FNUMBER, FRAW, E2FLONG, FDATE, EOR, PEOF};
#ifdef LCC
#define _int64 long long
#endif
#ifndef SYBASE
#ifndef SQLITE3
#ifndef INGRES
#ifdef DIY
/*
 * ORACLE OCI Elements
 */
struct cda_def {
    short v2_rc;                    /* ORACLE V.2 Return Code */
    unsigned short ft;              /* SQL Function Type      */
    unsigned long rpc;              /* Rows Processed Count   */
    unsigned short peo;             /* Parse Error Offset     */
    unsigned char fc;               /* OCI Function Code    ` */
    unsigned char fill1;            /* Filler                 */
    unsigned short rc;              /* ORACLE V.7 Return Code */
    unsigned char wrn;              /* Warning Flags          */
    unsigned char flg;              /* Reserved               */
    unsigned int d0;                /* Reserved               */
/*
 * Internal Representation of ROWID
 */
    struct {
        struct {
            unsigned long d1;
            unsigned short d2;
            unsigned char d3;
        } rd;
        unsigned long d4;
        unsigned long d5;
    } rid;
    unsigned int ose;               /* OSD Error             */
    unsigned char sysparm[27];      /* Reserved System Area  */
};
#define rcs7 d4
#define rcs8 d5
#define rcs5 d2
#ifndef __STDC__
#define signed
#define const
#endif
#else
#ifdef OR9
#include "oci.h"
#else
#ifdef __STDC__
#include "ociapr.h"
#else
#define signed
#define const
#include "ocidfn.h"
#include "ocikpr.h"
#endif
#endif
typedef struct cda_def cda_def;
typedef struct cda_def lda_def;
#endif
#endif
#endif
#endif
/*
 * Version of the SQLDA for use with these OCI and SAG features. This is
 * based on the ORACLE one.
 */
struct sqlda {
    char * base;                    /* Where variable data starts          */
    char * bound;                   /* Memory allocation test              */
    int arr;                        /* The assumed array size              */
    int all;                        /* The per-field allocation            */
#ifdef SYBASE
    CS_INT N;                    /* Count of Fields Sized               */
    CS_INT F;                    /* Count of Fields Found               */
    CS_CHAR ** S;                      /* Pointer to array of pointers to
                                       Variable Names                      */
    CS_INT * C;                  /* Pointer to array of name lengths    */
    CS_INT * T;                  /* Pointer to array of Field Types     */
    CS_INT * P;                  /* Precision                           */
    CS_INT * A;                  /* Scale                               */
    CS_INT * U;                  /* Nullability                         */
    CS_INT * L;                        /* Pointer to array of Field Lengths   */
    CS_CHAR ** V;                       /* Pointer to array of pointers to
                                       (arrays of) Variable Values         */
    CS_SMALLINT ** I;            /* Pointer to array of (arrays of) NULL
                                       Variable Indicators                 */
    CS_INT ** O;         /* Pointer to array of pointers to
                                       (arrays of) Output Lengths          */
    CS_INT ** R;         /* Pointer to array of pointers to
                                       (arrays of) Variable Return Codes   */
#else
    short int N;                    /* Count of Fields Sized               */
    short int F;                    /* Count of Fields Found               */
    char ** S;                      /* Pointer to array of pointers to
                                       Variable Names                      */
    short int * C;                  /* Pointer to array of name lengths    */
    short int * T;                  /* Pointer to array of Field Types     */
    short int * P;                  /* Precision                           */
    short int * A;                  /* Scale                               */
    short int * U;                  /* Nullability                         */
    int * L;                        /* Pointer to array of Field Lengths   */
    char ** V;                       /* Pointer to array of pointers to
                                       (arrays of) Variable Values         */
    short int ** I;                  /* Pointer to array of (arrays of) NULL
                                       Variable Indicators                 */
    unsigned short int ** O;         /* Pointer to array of pointers to
                                       (arrays of) Output Lengths          */
    unsigned short int ** R;         /* Pointer to array of pointers to
                                       (arrays of) Variable Return Codes   */
#ifdef OR9
    OCIBind **bindhp;                /* Pointer to array of pointers       */ 
#endif
#endif
};
typedef struct sqlda E2SQLDA;
#ifdef SYBASE
#define ORA_VARCHAR2 CS_VARCHAR_TYPE
#define ORA_VARCHAR  CS_VARCHAR_TYPE
#define ORA_CHAR CS_CHAR_TYPE
#define ORA_CHARZ CS_CHAR_TYPE
#define ORA_STRING CS_CHAR_TYPE
#define ORA_RAW CS_BINARY_TYPE
#define ORA_LONG CS_LONGCHAR_TYPE
#define ORA_LONG_RAW CS_LONGBINARY_TYPE
#define ORA_PACKED CS_DECIMAL_TYPE
#define ORA_INTEGER CS_INT_TYPE
#define ORA_UNSIGNED CS_LONG_TYPE
#define ORA_FLOAT    CS_FLOAT_TYPE
#define ORA_VARRAW CS_VARBINARY_TYPE
#else
#ifdef INGRES
#define ORA_VARCHAR2 IIAPI_VCH_TYPE
#define ORA_VARCHAR  IIAPI_VCH_TYPE
#define ORA_CHAR IIAPI_CHA_TYPE
#define ORA_CHARZ IIAPI_CHR_TYPE
#define ORA_STRING IIAPI_TXT_TYPE
#define ORA_RAW IIAPI_BYTE_TYPE
#define ORA_LONG IIAPI_LVCH_TYPE
#define ORA_LONG_RAW IIAPI_LBYTE_TYPE
#define ORA_PACKED IIAPI_DEC_TYPE
#define ORA_INTEGER IIAPI_INT_TYPE
#define ORA_UNSIGNED IIAPI_INT_TYPE
#define ORA_FLOAT    IIAPI_FLT_TYPE
#define ORA_VARRAW IIAPI_VBYTE_TYPE
#define ORA_NUMBER IIAPI_DEC_TYPE
#define ORA_DATE IIAPI_DTE_TYPE
#else
#ifdef SQLITE3
#define ORA_VARCHAR2 SQLITE_TEXT
#define ORA_VARCHAR  SQLITE_TEXT
#define ORA_CHAR SQLITE_TEXT
#define ORA_CHARZ SQLITE_TEXT
#define ORA_STRING SQLITE_TEXT
#define ORA_RAW SQLITE_TEXT
#define ORA_LONG SQLITE_TEXT
#define ORA_LONG_RAW SQLITE_TEXT
#define ORA_PACKED SQLITE_FLOAT
#define ORA_INTEGER SQLITE_INTEGER
#define ORA_UNSIGNED SQLITE_INTEGER
#define ORA_FLOAT  SQLITE_FLOAT
#define ORA_VARRAW SQLITE_TEXT
#define ORA_NUMBER SQLITE_FLOAT
#define ORA_DATE SQLITE_TEXT
#define SQLT_BLOB SQLITE_BLOB
#define SQLT_CLOB SQLITE_BLOB
#else
/*
 * Internal ORACLE Data Types
 */
#define ORA_VARCHAR2 1              /* Up to 2000 bytes long      */
#define ORA_NUMBER   2              /* Up to 22 bytes long        */
#define ORA_LONG     8              /* Up to 2**31 - 1 bytes long */
#define ORA_ROWID    11             /* 6 bytes long               */
#define ORA_DATE     12             /* 7 bytes long               */
#define ORA_RAW      23             /* Up to 255 byte long        */
#define ORA_LONG_RAW 24             /* Up to 2**31 - 1 bytes long */
#define ORA_CHAR     96             /* Up to 255 byte long        */
#define ORA_MLSLABEL 105            /* 4 bytes long               */
#define ORA_RAW_MLSLABEL 106        /* Up to 255 bytes long       */
/*
 * External ORACLE Data Types (where different to the above)
 */
#define ORA_INTEGER  3              /* Use for signed char, short  or long */
#define ORA_FLOAT    4              /* Use for float or double    */
#define ORA_STRING   5              /* Converts to null-terminated string */
#define ORA_VARNUM   6              /* Internal numeric representation? */
#define ORA_PACKED   7              /* Packed Decimal (not used much in C) */
#define ORA_VARCHAR  9              /* The traditional VARCHAR struct */
#define ORA_VARRAW   15             /* A VARCHAR-like way of returning RAW */
#define ORA_UNSIGNED 68             /* Unsigned long int          */
#define ORA_DISPLAY  91             /* COBOL Display data type    */
#define ORA_LONG_VARCHAR 94         /* Long int + byte data       */
#define ORA_LONG_VARRAW  95         /* Long int + byte data       */
#define ORA_CHARZ    97             /* Null terminated char data  */
/*
 * Logon Data Area plus copy of connect details.
 */
#ifndef HDA_SIZE
#ifdef OSF
#define HDA_SIZE 512
#else
#define HDA_SIZE 256
#endif
#endif
#endif
#endif
#endif
struct sess_con {
char * uid_pwd;                     /* Pointer to login string    */
#ifdef SYBASE
/*
 * SYBASE Context details
 */
char * appname;                     /* Pointer to program name string      */
CS_CONTEXT * context;               /* Context                             */
CS_CONNECTION * connection;         /* Connexion                           */
CS_COMMAND * cmd;                   /* Command handle used without cursor  */
CS_RETCODE ret_status;              /* Connection return status            */
char message_block[65536];          /* Incoming message accumulation       */
char *msgp;                         /* Next message location               */
#else
#ifdef INGRES
/*
 * Ingres Context details
 */
int ret_status;
II_PTR connHandle;
II_PTR tranHandle;
IIAPI_GENPARM genParm;
II_LONG errorCode;
int auto_state;
int curs_no;
int port;
#else
#ifdef OR9
/*
 * Things that have to be initialised before doing anything useful
 */
OCIEnv *envhp;
OCISvcCtx *svchp;
OCIError *errhp;
OCIServer *srvhp;
OCISession *authp;
int       ret_status;               /* Returned status            */
#else
#ifdef SQLITE3
sqlite3 *srvhp;
int       ret_status;               /* Returned status            */
#else
/*
 * ORACLE session details
 */
lda_def lda;                        /* The login data area        */
char hda[HDA_SIZE];                 /* The host data area         */
int       ret_status;               /* Returned status            */
#endif
#endif
#endif
#endif
HASH_CON *ht;                       /* The cursor hash table      */
struct csmacro csmacro;             /* The variable hash table    */
int cost_cnt;                       /* The number of costs        */
struct cost_accum * costs;          /* The array of costs         */
char description[80];               /* A description, for display */
};
extern struct sess_con * con;
/*
 * Accumulators for statistics gathering
 */
struct cost_accum {
char * stat_no;
char * stat_name;
double last_value;
double cur_value;
};
/*
 * Structure for managing binds and defines during the creation
 * of the descriptors.
 */
struct bv {
    struct bv * next;
    char * bname;
#ifdef SYBASE
   CS_INT blen;
   CS_INT dbsize;
   CS_INT dbtype;
   CS_INT dsize;
   CS_INT prec;
   CS_INT scale;
   CS_INT nullok;
#else
    int blen;
    int dbsize;
    short dbtype;
    int dsize;
    short prec;
    short scale;
    short nullok;
#ifdef OR9
    char * tname;
    int tlen;
    char * sname;
    int slen;
#else
#ifdef INGRES
    int vlen;
    char *val;
#else
#ifdef SQLITE3
    int seq;
    char *val;
#endif
#endif
#endif
#endif
};
/*
 * Control structure for dynamic statements. This includes the cursor
 * details, and a key to use for repeated execution tests; this key is
 * hashed.
 */
struct dyn_con {
    int cnum;                 /* Cursor Number (for re-execution)       */
    struct sess_con * con;    /* ORACLE Session Details                 */
#ifdef SYBASE
    int need_dynamic;         /* Flag for data type adjustment       */
    CS_COMMAND * cmd;         /* Command Handle                      */
    CS_LOCALE * locale;       /* Locale (should be constant)         */
    CS_INT res_type;          /* Results type                        */
#else
#ifdef INGRES
    char hash_key[80];        /* Hash key for global hash of statements */
IIAPI_QUERYTYPE queryType;
II_PTR stmtHandle;
II_PTR reptHandle;
II_PTR cursorHandle;
IIAPI_QUERYPARM     queryParm;
IIAPI_AUTOPARM      autoparm;
IIAPI_GETDESCRPARM  getDescrParm;
IIAPI_GETCOLPARM    getColParm;
IIAPI_GETQINFOPARM  getQInfoParm;
IIAPI_SETDESCRPARM  setDescrParm;
IIAPI_PUTPARMPARM   putParmParm;
II_INT4 repti1;
II_INT4 repti2;
char reptid[64];
#else
#ifdef SQLITE3
sqlite3_stmt * stmthp;
int sel_cols;
int rows;
#else
#ifdef OR9
    OCIStmt *stmthp;
#else
    cda_def cda;              /* Cursor Data Area                    */
#endif
#endif
#endif
#endif
    char * statement;         /* Text of SQL Statement               */
    E2SQLDA     *bdp;           /* -> Descriptor used for BIND vars    */
    E2SQLDA     *sdp;           /* -> Descriptor used for SELECT vars  */
#ifdef SYBASE
    CS_INT     *sdt;           /* -> arr of original DESCRIBE'd types */
#else
    short     *sdt;           /* -> arr of original DESCRIBE'd types */
#endif
    char      *scram_flags;   /* -> arr of scramble flags            */
    int is_sel;               /* This statement is a SELECT          */
    int reb_flag;             /* Must reparse and bind again         */
    short int *sb_map;        /* Counted lists of the number of
                                 references in the bind descriptor
                                 list to columns in the select
                                 descriptor list                     */
    int       sdtl;           /* no of entries in sdt[] and
                                            scram_flags              */
    int       bd_size;        /* Size of Bind variable descriptor    */
    int       bv_size;        /* Max no of chars in Bind Var name    */
    int       sd_size;        /* Size of Select list descriptor      */
    int       sv_size;        /* Max no chars in Select List colnames*/
    int       ind_size;       /* Max no chars in indicator names     */
#ifdef SYBASE
    CS_RETCODE   ret_status;     /* Returned status                     */
#else
    int       ret_status;     /* Returned status                     */
#endif
    int       to_do;          /* Number remaining for processing     */
    int       so_far;         /* Number retrieved so far             */
    int       cur_ind;        /* The current array position          */
    int       fld_ind;        /* The current field position          */
    short int *cur_map;       /* The current field position in the
                                 select/bind map                     */
    int       chars_read;     /* Count of characters read by selects */
    int       chars_sent;     /* Length of SQL Statements            */
    int       rows_read;      /* Count of rows processed             */
    int       rows_sent;      /* Count of rows processed             */
    int       fields_read;    /* Count of fields read                */
    int       fields_sent;    /* Count of fields sent                */
    struct mem_con * anchor;  /* Memory owned by this statement      */
#ifdef INGRES
    struct bv * head_bv;
    struct bv * tail_bv;
#endif
};
/*
 * Structure used for controlling rendition and editing of data. Most of this
 * is not used at present, since we only want to display.
 */
struct disp_con {
    struct dyn_con *dsel;    /* Statement that sources the data       */
    struct dyn_con *dins;    /* Statement that handles new rows       */
    struct dyn_con *dupd;    /* Statement that handles changed rows   */
    struct dyn_con *ddel;    /* Statement that handles deleted rows   */
    E2SQLDA     *bdp;          /* Descriptor that holds current values  */
    unsigned int * col_flags;        /* Bitmap with output control flags      */
    char * title;            /* Optional heading                      */
    int       to_do;         /* Number remaining for processing       */
    int       so_far;        /* Number retrieved so far               */
    int       cur_ind;       /* The current array position            */
    int       fld_ind;       /* The current field position            */
};
/*
 * Structure for managing a dictionary of bind details, so that data
 * type information for input bind variables can be looked up.
 */
struct dict_con {
    struct bv * anchor;         /* Anchor for chain of member bind variables */
    HASH_CON * bvt;             /* Hash table of member bind variables       */
};
struct dict_con * new_dict();  /* Create a new dictionary             */
struct dict_con * set_cur_dict();     /* Set the current dictionary          */
struct dict_con * get_cur_dict();     /* Get the current dictionary          */
#define TRAIL_SPACE_STRIP(x,y)\
{ char *_y; for (_y=(x)+(y)-1;\
 (y)>0 &&(*_y=='\0'||*_y==' '||*_y=='\t');*_y-- ='\0',(y)--);}
#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

struct bv * find_bv();
/*
 * Cursor numbers allocated for internal reasons
 */
#define COST_CURS -1
#define NONSP_CURS -2
#define DYN_SYB_CURS -3
#define DYN_EXP_CURS -4
/*************************************************************************
 * Functions defined in e2sqllib.c
 */
void exec_sel();
void get_cols();
void prep_dml();
void exec_dml();
void dyn_forget();
void dyn_reset();
struct sess_con * or_connect();
void ini_bind_vars();
struct dyn_con * dyn_init();
struct sess_con * dyn_connect();
int dyn_disconnect();
void dyn_note();
void dyn_descr_note();
struct dyn_con * dyn_find();
enum tok_id get_tok();
void form_print();
void bind_print();
char * dbms_rowid();
void col_dump();
void col_disc();
void dyn_fetch();
void dyn_reinit();
void dyn_kill();
enum tok_id dyn_exec();
void desc_bind();
short int quote_stuff();
short int out_field();
void desc_sel();
void add_field();
enum tok_id prem_eof();
void sort_out();
void flat_print();
char * row_str();
char * col_head_str();
void flat_print();
/******************************************************************
 * Parser data
 */
extern char * tbuf;
extern char * tlook;
extern int tlen;
extern enum tok_id look_tok;
extern enum look_status look_status;
/*
 * Functions that must be defined by the user of e2sqllib.c
 */
char * scramble();
void scarper();

extern int scram_cnt;       /* Count of to-be-scrambled strings */
extern char * scram_cand[]; /* Candidate scramble patterns */
void desc_print();
void type_print();
E2SQLDA    *sqlald();       /* Allocate Descriptor             */
void      sqlclu();       /* Destroy Descriptor              */
struct disp_con * disp_new();
void disp_reset();
void res_process();
void set_long();
#ifndef LINUX
extern char * strdup();
#endif
#ifdef INGRES
int dyncomp();
int dynhash();
#endif
void dyn_dump();
void dbms_error();
extern FILE * fdopen();
/*
 * Bitmap macros
 */
#define E2BIT_TEST(bitmap,index)\
         (*((bitmap) + ((index)/32)) & (1 << ((index) % 32)))
#define E2BIT_SET(bitmap,index)\
         (*((bitmap) + ((index)/32)) |= (1 << ((index) % 32)))
#define E2BIT_CLEAR(bitmap,index)\
         (*((bitmap) + ((index)/32)) &= ~(1 << ((index) % 32)))
#endif
