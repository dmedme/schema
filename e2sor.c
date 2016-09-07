/**************************************************************************
 * e2sor.c - Code to compare schemas for different users, potentially in
 * different databases
 */
#ifdef NT4
#include <windows.h>
#else
#include <stdio.h>
#include <time.h>
#endif
#include "tabdiff.h"
#include "difflib.h"
int scram_cnt;                /* Count of to-be-scrambled strings */
char * tbuf;
char * scram_cand[1];         /* Candidate scramble patterns */
char * tlook;
int tlen;
enum tok_id look_tok;
enum look_status look_status;

static struct sess_con * sess_con[2];            /* The pair of sessions */ 
#define FDB 0
#define SDB 1
#define EOF_FDB 0x1
#define EOF_SDB 0x2
#define HIGH_FDB 0x4
#define LOW_FDB 0x8
#define MATCH_FDB 0x10
static void report_db();
static void objcompare();
/**************************************************************************
 * Find the (probable) maximum size of a long
 */
#define CLNG_SEL 1
static struct dyn_con * dlng_sel[2];
static char * lng_sel = "select max(text_length) from dba_views;\n";
static int llng;
static char * plng;
/**************************************************************************
 * Get the lists of things
 *
 * Constraints
 */ 
#define CCON_SEL 2
static struct dyn_con * dcon_sel[2];
static char * con_sel = "select\n\
    a.table_name \"Table Name\",\n\
    a.constraint_type \"Type\",\n\
    a.constraint_name \"Constraint Name\",\n\
    a.r_owner \"Related Constraint Owner\",\n\
    a.r_constraint_name \"Related Constraint Name\",\n\
    a.delete_rule \"Delete Rule\",\n\
    a.status \"Status\",\n\
    a.search_condition \"Search Condition\",\n\
    b.column_name \"Column Name\"\n\
from\n\
    dba_constraints a,\n\
    dba_cons_columns b\n\
where\n\
    a.table_name = b.table_name(+)\n\
and a.owner = b.owner(+)\n\
and a.constraint_name = b.constraint_name(+)\n\
and a.owner = :owner\n\
order by 1, 2, 3, 4, b.position\n";
static int lcon[2][9];
static char * pcon[2][9];
static struct disp_con * disp_con[2];
/*
 * Indexes
 */
#define CIND_SEL 3
static struct dyn_con * dind_sel[2];
static char * ind_sel = "select\n\
    a.table_name \"Table Name\",\n\
    a.uniqueness \"Unique?\",\n\
    a.index_name \"Index Name\",\n\
    b.column_name \"Column Name\"\n\
from\n\
    dba_indexes a,\n\
    dba_ind_columns b\n\
where\n\
    a.owner = b.index_owner\n\
and a.index_name = b.index_name\n\
and a.table_name = b.table_name\n\
and b.index_owner = :owner\n\
order by 1, 2, 3, b.column_position\n";
static int lind[2][4];
static char * pind[2][4];
static struct disp_con * disp_ind[2];
/*
 * Master list of objects
 *
 * Constraints, indexes and triggers are picked up at the table level
 */ 
#define COBJ_SEL 4
static struct dyn_con * dobj_sel[2];
static char * obj_sel = "select\n\
    object_type \"Object Type\",\n\
    object_name \"Object Name\"\n\
from\n\
    dba_objects\n\
where\n\
    owner = :owner\n\
and object_type != 'INDEX'\n\
and object_type != 'CONSTRAINT'\n\
and object_type != 'TRIGGER'\n\
order by 1,2\n";
static int lobj[2][2];
static char * pobj[2][2];
static struct disp_con * disp_obj;
/*
 * PL/SQL Packages, Package Bodies, Procedures and Functions
 */
#define CPLS_SEL 5
static struct dyn_con * dpls_sel[2];
static char * pls_sel = "select\n\
    type \"PL/SQL Type\",\n\
    name \"PL/SQL Name\",\n\
    text \"Source\"\n\
from\n\
    dba_source\n\
where\n\
    owner = :owner\n\
order by 1,2, line\n";
static int lpls[2][3];
static char * ppls[2][3];
static struct disp_con * disp_pls[2];
/*
 * Synonyms
 */
#define CSYN_SEL 6
static struct dyn_con * dsyn_sel[2];
static char * syn_sel = "select\n\
    synonym_name \"Synonym\",\n\
    table_owner \"Owner\",\n\
    table_name \"Table Name\",\n\
    db_link \"Database Link\"\n\
from\n\
    dba_synonyms\n\
where\n\
   owner = :owner\n\
order by 1\n";
static int lsyn[2][4];
static char * psyn[2][4];
static struct disp_con * disp_syn[2];
/*
 * Tables
 */
#define CTAB_SEL 7
static struct dyn_con * dtab_sel[2];
static char * tab_sel = "select\n\
    table_name \"Table Name\",\n\
    column_name \"Column Name\",\n\
    data_type  \"Type\",\n\
    data_length \"Length\",\n\
    data_precision \"Precision\",\n\
    data_scale \"Scale\",\n\
    nullable \"Nullable?\",\n\
    data_default \"Default\"\n\
from\n\
    dba_tab_columns\n\
where\n\
    owner = :owner\n\
order by 1,column_id\n";
static int ltab[2][8];
static char * ptab[2][8];
static struct disp_con * disp_tab[2];
/*
 * Triggers
 * 
 * We ignore the TRIGGER_COLS table because I cannot think what to do with it
 */
#define CTRG_SEL 8
static struct dyn_con * dtrg_sel[2];
static char * trg_sel = "select\n\
    table_name \"Table Name\",\n\
    trigger_type \"Type\",\n\
    triggering_event \"Fired By\",\n\
    trigger_name \"Trigger Name\",\n\
    referencing_names \"References\",\n\
    when_clause \"When Clause\",\n\
    status \"Status\",\n\
    trigger_body \"Trigger Body\"\n\
from\n\
    dba_triggers\n\
where\n\
    table_owner = :owner\n\
order by 1, 2, 3, 4\n";
static int ltrg[2][8];
static char * ptrg[2][8];
static struct disp_con * disp_trg[2];
/*
 * List of users (For pull-down list population)
 */
#define CUSR_SEL 9
static struct dyn_con * dusr_sel[2];
static char * usr_sel = "select\n\
    username \"User\"\n\
from\n\
    dba_users\n\
where\n\
    username like :owner\n\
order by 1\n";
static int lusr[2];
static char * pusr[2];
/*
 * Views
 */
#define CVIEW_SEL 10
static struct dyn_con * dview_sel[2];
static char * view_sel = "select\n\
    view_name \"View Name\",\n\
    text \"Definition\"\n\
from\n\
    dba_views\n\
where\n\
    owner = :owner\n\
order by 1\n";
static int lview[2][2];
static char * pview[2][2];
static struct disp_con * disp_view[2];
/****************************************************************************
 * Set up the SQL statements
 */
static void open_all_sql()
{
int i;
    set_def_binds(1,20);
    set_long(16384);
    for (i = 0; i < 2; i++)
    {
        curse_parse(sess_con[i], &dlng_sel[i], CLNG_SEL, lng_sel);
        curse_parse(sess_con[i], &dcon_sel[i], CCON_SEL, con_sel);
        curse_parse(sess_con[i], &dind_sel[i], CIND_SEL, ind_sel);
        curse_parse(sess_con[i], &dobj_sel[i], COBJ_SEL, obj_sel);
        curse_parse(sess_con[i], &dpls_sel[i], CPLS_SEL, pls_sel);
        curse_parse(sess_con[i], &dsyn_sel[i], CSYN_SEL, syn_sel);
        curse_parse(sess_con[i], &dtab_sel[i], CTAB_SEL, tab_sel);
        curse_parse(sess_con[i], &dtrg_sel[i], CTRG_SEL, trg_sel);
        curse_parse(sess_con[i], &dusr_sel[i], CUSR_SEL, usr_sel);
        curse_parse(sess_con[i], &dview_sel[i], CVIEW_SEL, view_sel);
    }
    return;
}
/****************************************************************************
 * Load the bind variables into a dictionary
 */
static void load_dict()
{
struct dict_con * dict = new_dict( 20);
static int called = 0;
    if (called)
        return;
    else
        called = 1;
    dict_add(dict,":owner",ORA_CHAR,  35);
    (void) set_cur_dict(dict);
    return;
}
/*
 * Get the child objects to the same place as the main one
 */
void align_sub_objects(whch, typ_len, typ, obj_len, obj)
int whch;
int typ_len;
char * typ;
int obj_len;
char * obj;
{
int ret = 1;
int i;
    if (!strncmp(typ,"VIEW",typ_len))
    {
/*
 * Position to the view text
 */
        while (lview[whch][0]
            && (mulcmp(obj_len, obj, lview[whch][0], pview[whch][0]) > 0))
        {
            if (! dyn_loc_raw(dview_sel[whch], &(lview[whch][0]),
                        &(pview[whch][0])))
                lview[whch][0] = 0;
        }
        disp_reset(disp_view[whch]);
        while (lview[whch][0]
            && (mulcmp(obj_len, obj, lview[whch][0], pview[whch][0]) == 0))
        {
            for (i = 0; i < disp_view[whch]->bdp->N; i++)
                ret = disp_add(disp_view[whch], lview[whch][i], pview[whch][i]);
            dyn_forget(dview_sel[whch]);
            if (!ret || ! dyn_loc_raw(dview_sel[whch], &(lview[whch][0]),
                        &(pview[whch][0])))
                lview[whch][0] = 0;
        }
    }
    else
    if (!strncmp(typ,"TABLE",typ_len))
    {
/*
 * Position the index columns, the constraint definitions and the triggers,
 * as well as the table columns. 
 */
        while (ltab[whch][0]
            && (mulcmp(obj_len, obj, ltab[whch][0], ptab[whch][0]) > 0))
        {
/*
 * These will be views with columns. We might think about doing this better.
 */
            if (! dyn_loc_raw(dtab_sel[whch], &(ltab[whch][0]),
                        &(ptab[whch][0])))
                ltab[whch][0] = 0;
        }
        disp_reset(disp_tab[whch]);
        while (ltab[whch][0]
            && (mulcmp(obj_len, obj, ltab[whch][0], ptab[whch][0]) == 0))
        {
            for (i = 0; i < disp_tab[whch]->bdp->N; i++)
                ret = disp_add(disp_tab[whch], ltab[whch][i], ptab[whch][i]);
            dyn_forget(dtab_sel[whch]);
            if (!ret || ! dyn_loc_raw(dtab_sel[whch], &(ltab[whch][0]),
                        &(ptab[whch][0])))
            {
#ifdef DEBUG
                fputs("Table tail\n",stderr);
                desc_print(stderr, (FILE *) NULL,
                    dtab_sel[whch]->sdp, 20, (char *) NULL);
#endif
                ltab[whch][0] = 0;
            }
        }
        while (lind[whch][0]
            && (mulcmp(obj_len, obj, lind[whch][0], pind[whch][0]) > 0))
        {
            if (! dyn_loc_raw(dind_sel[whch], &(lind[whch][0]),
                        &(pind[whch][0])))
                lind[whch][0] = 0;
        }
        disp_reset(disp_ind[whch]);
        while (lind[whch][0]
            && (mulcmp(obj_len, obj, lind[whch][0], pind[whch][0]) == 0))
        {
            for (i = 0; i < disp_ind[whch]->bdp->N; i++)
                ret = disp_add(disp_ind[whch], lind[whch][i], pind[whch][i]);
            dyn_forget(dind_sel[whch]);
            if (!ret || ! dyn_loc_raw(dind_sel[whch], &(lind[whch][0]),
                        &(pind[whch][0])))
                lind[whch][0] = 0;
        }
        while (lcon[whch][0]
            && (mulcmp(obj_len, obj, lcon[whch][0], pcon[whch][0]) > 0))
        {
            if (! dyn_loc_raw(dcon_sel[whch], &(lcon[whch][0]),
                        &(pcon[whch][0])))
                lcon[whch][0] = 0;
        }
        disp_reset(disp_con[whch]);
        while (lcon[whch][0]
            && (mulcmp(obj_len, obj, lcon[whch][0], pcon[whch][0]) == 0))
        {
            for (i = 0; i < disp_con[whch]->bdp->N; i++)
                ret = disp_add(disp_con[whch], lcon[whch][i], pcon[whch][i]);
            dyn_forget(dcon_sel[whch]);
            if (!ret || ! dyn_loc_raw(dcon_sel[whch], &(lcon[whch][0]),
                        &(pcon[whch][0])))
                lcon[whch][0] = 0;
        }
        while (ltrg[whch][0]
            && (mulcmp(obj_len, obj, ltrg[whch][0], ptrg[whch][0]) > 0))
        {
            if (! dyn_loc_raw(dtrg_sel[whch], &(ltrg[whch][0]),
                        &(ptrg[whch][0])))
                ltrg[whch][0] = 0;
        }
        disp_reset(disp_trg[whch]);
        while (ltrg[whch][0]
            && (mulcmp(obj_len, obj, ltrg[whch][0], ptrg[whch][0]) == 0))
        {
            for (i = 0; i < disp_trg[whch]->bdp->N; i++)
                ret = disp_add(disp_trg[whch], ltrg[whch][i], ptrg[whch][i]);
            dyn_forget(dtrg_sel[whch]);
            if (!ret || ! dyn_loc_raw(dtrg_sel[whch], &(ltrg[whch][0]),
                        &(ptrg[whch][0])))
                ltrg[whch][0] = 0;
        }
    }
    else
    if (!strncmp(typ,"FUNCTION",typ_len)
     || !strncmp(typ,"PACKAGE",typ_len)
     || !strncmp(typ,"PACKAGE BODY",typ_len)
     || !strncmp(typ,"PROCEDURE",typ_len))
    {
/*
 * Position the source code.
 */
        while (lpls[whch][0]
            && ((mulcmp(typ_len, typ, lpls[whch][0], ppls[whch][0]) < 0)
            || (!mulcmp(typ_len, typ, lpls[whch][0], ppls[whch][0])
            && (mulcmp(obj_len, obj, lpls[whch][1], ppls[whch][1]) < 0))))
        {
            if (! dyn_loc_raw(dpls_sel[whch], &(lpls[whch][0]),
                        &(ppls[whch][0])))
                lpls[whch][0] = 0;
        }
        disp_reset(disp_pls[whch]);
        while (lpls[whch][0]
            && (mulcmp(typ_len, typ, lpls[whch][0], ppls[whch][0]) == 0)
            && (mulcmp(obj_len, obj, lpls[whch][1], ppls[whch][1]) == 0))
        {
            for (i = 0; i < disp_pls[whch]->bdp->N; i++)
                ret = disp_add(disp_pls[whch], lpls[whch][i], ppls[whch][i]);
            dyn_forget(dpls_sel[whch]);
            if (!ret || ! dyn_loc_raw(dpls_sel[whch], &(lpls[whch][0]),
                        &(ppls[whch][0])))
                lpls[whch][0] = 0;
        }
    }
    else
    if (!strncmp(typ,"SYNONYM",typ_len))
    {
        while (lsyn[whch][0]
            && (mulcmp(obj_len, obj, lsyn[whch][0], psyn[whch][0]) < 0))
        {
            if (! dyn_loc_raw(dsyn_sel[whch], &(lsyn[whch][0]),
                        &(psyn[whch][0])))
                lsyn[whch][0] = 0;
        }
        disp_reset(disp_syn[whch]);
        while (lsyn[whch][0]
            && (mulcmp(obj_len, obj, lsyn[whch][0], psyn[whch][0]) == 0))
        {
            for (i = 0; i < disp_syn[whch]->bdp->N; i++)
                ret = disp_add(disp_syn[whch], lsyn[whch][i], psyn[whch][i]);
            dyn_forget(dsyn_sel[whch]);
            if (!ret || ! dyn_loc_raw(dsyn_sel[whch], &(lsyn[whch][0]),
                        &(psyn[whch][0])))
                lsyn[whch][0] = 0;
        }
    }
    return;
}
/******************************************************************************
 * Main control. The programming logic is as follows:
 * - Get the user options:
 *   -  Get the two database connections
 *   -  Get the two users to compare
 *   -  Get the name of an exception file to create
 *   (-  Identify which things to compare. For now, we do them all) 
 * - Load the dictionary (only one data value at the moment!)
 * - Parse all the SQL in both sessions
 * - Get the first record from each.
 *   - Get the element from DBA_OBJECTS (obj_sel) first
 *   - Align it with its various sub-records; table, package, etc. etc.
 * - Now do the serial merge.
 *   - No match, dump out the details (including the subsidiary elements)
 *     for the extra one to the output file.  We assemble in memory a block
 *     that can be strcmp()ed, and dumped to the output file, or rendered to
 *     a List View control. The SQLDA structure looks suitable, as we have
 *     functions for adding and accessing the various elements.
 *   (- Create elements in the Outline window)
 *   - If there is a match, proceed to the sub-records, and compare these.
 */
int schm_cmp_main(frst, scnd, owner1, owner2, ofname)
struct sess_con * frst;
struct sess_con * scnd;
char * owner1;
char * owner2;
char * ofname;
{
FILE * new_channel;
int merge_flag;
int i;
time_t clck;
/*
 * Initialise
 */
    if (!strcmp(ofname, "-"))
        new_channel = stdout;
    else
    if ((new_channel = fopen (ofname,"wb")) == (FILE *) NULL)
            /* open the output file */
    {    /* logic error; cannot create output member file */
        scarper(__FILE__, __LINE__, "Cannot create output file! Aborting\n");
        return 0;
    }
    if (frst == NULL && scnd != NULL)
        frst = scnd;
    else
    if (frst != NULL && scnd == NULL)
        scnd = frst;
    if (owner1 == NULL && owner2 != NULL)
        owner1 = owner2;
    else
    if (owner1 != NULL && owner2 == NULL)
        owner2 = owner1;
    sess_con[FDB] = frst;
    sess_con[SDB] = scnd;
    clck = time(0);
    if (owner1 != owner2)
        fprintf(new_channel, "Schema Comparison for %s via %s and %s via %s\n\
run %s\n", owner1, sess_con[FDB]->description,
                owner2, sess_con[SDB]->description, ctime(&clck));
    else
        fprintf(new_channel, "Schema Dump for %s via %s\n\
run %s\n", owner1, sess_con[FDB]->description,
                ctime(&clck));
    fflush(new_channel);
    open_all_sql();
    load_dict();
    add_bind(dobj_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dtab_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dcon_sel[FDB],  FIELD, strlen(owner1), owner1);
    add_bind(dind_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dpls_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dsyn_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dtrg_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dview_sel[FDB], FIELD, strlen(owner1), owner1);
    add_bind(dobj_sel[SDB], FIELD, strlen(owner2), owner2);
    add_bind(dtab_sel[SDB], FIELD, strlen(owner2), owner2);
    add_bind(dcon_sel[SDB],  FIELD, strlen(owner2), owner2);
    add_bind(dind_sel[SDB], FIELD, strlen(owner2), owner2);
    add_bind(dpls_sel[SDB], FIELD, strlen(owner2), owner2);
    add_bind(dsyn_sel[SDB], FIELD, strlen(owner2), owner2);
    add_bind(dtrg_sel[SDB], FIELD, strlen(owner2), owner2);
    add_bind(dview_sel[SDB], FIELD, strlen(owner2), owner2);
/*
 * Execute the object select statements and read the first record on each
 */
    merge_flag = 0;
    for (i = 0; i < ((owner1 == owner2) ? 1 : 2 ); i++)
    {
        exec_dml(dobj_sel[i]);
        if (sess_con[i]->ret_status != 0)
        {
            scarper(__FILE__,__LINE__,
                                  "Unexpected Error fetching objects");
            return 0;
        }
        dobj_sel[i]->so_far = 0;
        if (!dyn_loc_raw(dobj_sel[i],&(lobj[i][0]),&(pobj[i][0])))
        {
            if (i == 0)
                merge_flag |= EOF_FDB;
            else
                merge_flag |= EOF_SDB;
        }
        else
        {
            exec_dml(dtab_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching table columns");
                return 0;
            }
            dtab_sel[i]->so_far = 0;
            if (! dyn_loc_raw(dtab_sel[i],&(ltab[i][0]),&(ptab[i][0])))
                ltab[i][0] = 0;
            else
            {
                disp_tab[i] = disp_new((dtab_sel[i])->sdp, 1024);
                disp_tab[i]->dsel = dtab_sel[i];
                *(disp_tab[i]->col_flags) = 0xfffe;
            }
            exec_dml(dind_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching index columns");
                return 0;
            }
            dind_sel[i]->so_far = 0;
            if (! dyn_loc_raw(dind_sel[i],&(lind[i][0]),&(pind[i][0])))
                lind[i][0] = 0;
            else
            {
                disp_ind[i] = disp_new((dind_sel[i])->sdp, 254);
                disp_ind[i]->dsel = dind_sel[i];
                *(disp_ind[i]->col_flags) = 0xfffe;
                disp_ind[i]->title = strdup("INDEXES");
            }
            exec_dml(dcon_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching constraint columns");
                return 0;
            }
            dcon_sel[i]->so_far = 0;
            if (! dyn_loc_raw(dcon_sel[i],&(lcon[i][0]),&(pcon[i][0])))
                lcon[i][0] = 0;
            else
            {
                disp_con[i] = disp_new((dcon_sel[i])->sdp, 254);
                disp_con[i]->dsel = dcon_sel[i];
                *(disp_con[i]->col_flags) = 0xfffe;
                disp_con[i]->title = strdup("CONSTRAINTS");
            }
            exec_dml(dpls_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching PL/SQL source");
                return 0;
            }
            dpls_sel[i]->so_far = 0;
            if (!dyn_loc_raw(dpls_sel[i],&(lpls[i][0]),&(ppls[i][0])))
                lpls[i][0] = 0;
            else
            {
                disp_pls[i] = disp_new((dpls_sel[i])->sdp, 16384);
                disp_pls[i]->dsel = dpls_sel[i];
                *(disp_pls[i]->col_flags) = 0xfffe;
            }
            exec_dml(dsyn_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching synonym details");
                return 0;
            }
            dsyn_sel[i]->so_far = 0;
            if (! dyn_loc_raw(dsyn_sel[i],&(lsyn[i][0]),&(psyn[i][0])))
                lsyn[i][0] = 0;
            else
            {
                disp_syn[i] = disp_new((dsyn_sel[i])->sdp, 1);
                disp_syn[i]->dsel = dsyn_sel[i];
                *(disp_syn[i]->col_flags) = 0xfffe;
            }
            exec_dml(dtrg_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching trigger details");
                return 0;
            }
            dtrg_sel[i]->so_far = 0;
            if (! dyn_loc_raw(dtrg_sel[i],&(ltrg[i][0]),&(ptrg[i][0])))
                ltrg[i][0] = 0;
            else
            {
                disp_trg[i] = disp_new((dtrg_sel[i])->sdp, 254);
                disp_trg[i]->dsel = dtrg_sel[i];
                *(disp_trg[i]->col_flags) = 0xfffe;
                disp_trg[i]->title = strdup("TRIGGERS");
            }
            exec_dml(dview_sel[i]);
            if (sess_con[i]->ret_status != 0)
            {
                scarper(__FILE__,__LINE__,
                         "Unexpected Error fetching view text");
                return 0;
            }
            dview_sel[i]->so_far = 0;
            if (! dyn_loc_raw(dview_sel[i],&(lview[i][0]),&(pview[i][0])))
                lview[i][0] = 0;
            else
            {
                disp_view[i] = disp_new((dview_sel[i])->sdp, 1);
                disp_view[i]->dsel = dview_sel[i];
                *(disp_view[i]->col_flags) = 0xfffe;
            }
/*
 * Set up the other statements that are to be read in step
 */
            align_sub_objects(i, lobj[i][0],pobj[i][0], lobj[i][1],pobj[i][1]);
        }
    }
    if (owner1 == owner2)
        merge_flag = EOF_SDB;
/*******************************************************************************
 *     Main Control; loop - merge the two lists until both are
 *     exhausted
 ******************************************************************************/
    while  ((merge_flag & (EOF_FDB |  EOF_SDB))
            != (EOF_FDB | EOF_SDB))
    {
#ifdef DEBUG
        fprintf(stderr, "1 %*.*s %*.*s 2 %*.*s %*.*s\n",
        lobj[FDB][0],lobj[FDB][0], pobj[FDB][0],
        lobj[FDB][1],lobj[FDB][1], pobj[FDB][1],
        lobj[SDB][0],lobj[SDB][0],pobj[SDB][0],
        lobj[SDB][1],lobj[SDB][1],pobj[SDB][1]);
#endif
        if  ((merge_flag & (EOF_FDB |  EOF_SDB)) == 0)
        {
/*
 * If the two streams are still alive, compare the type and name
 */
        int name_match = mulcmp(lobj[FDB][0], pobj[FDB][0],
                                lobj[SDB][0],pobj[SDB][0]);
#ifdef DEBUG
            printf("Compare: %d %-.*s %-.*s", name_match, 
                      lobj[FDB][0], pobj[FDB][0],lobj[SDB][0],pobj[SDB][0]);
#endif
            if (!name_match)
            {
                name_match = mulcmp(lobj[FDB][1],
                                    pobj[FDB][1],lobj[SDB][1],pobj[SDB][1]);
#ifdef DEBUG
                printf(" Compare: %d %-.*s %-.*s", name_match, 
                      lobj[FDB][1], pobj[FDB][1],lobj[SDB][1],pobj[SDB][1]);
#endif
            }
            if (!name_match)
                merge_flag =  MATCH_FDB;
            else
                merge_flag = (name_match > 0) ? HIGH_FDB : LOW_FDB;
#ifdef DEBUG
            printf(" merge_flag: %d\n", merge_flag);
#endif
        }
        else
            merge_flag &= ~(LOW_FDB | HIGH_FDB);

        if (merge_flag & (EOF_SDB | LOW_FDB))
        {
            fprintf(new_channel, "\n%*.*s %*.*s is unique to %s via %s\n",
                lobj[FDB][0], lobj[FDB][0], pobj[FDB][0], 
                lobj[FDB][1], lobj[FDB][1], pobj[FDB][1], 
                owner1, sess_con[FDB]->description);
            fflush(new_channel);
/*
 * If we have something in the first database that is not in the second
 * write out the details of the first database stuff
 */
            report_db(new_channel, FDB);
            fflush(new_channel);
            if (!dyn_loc_raw(dobj_sel[FDB],&(lobj[FDB][0]),&(pobj[FDB][0])))
                merge_flag |= EOF_FDB;
            else
                align_sub_objects(FDB, lobj[FDB][0],pobj[FDB][0],
                     lobj[FDB][1],pobj[FDB][1]);
        }
        else
        if (merge_flag & (EOF_FDB | HIGH_FDB))
        {
            fprintf(new_channel, "\n%*.*s %*.*s is unique to %s via %s\n",
                lobj[SDB][0], lobj[SDB][0], pobj[SDB][0], 
                lobj[SDB][1], lobj[SDB][1], pobj[SDB][1], 
                owner2, sess_con[SDB]->description);
/*
 * If we have something in the second database that is not in the first
 * write out the details of the second database stuff
 */
            fflush(new_channel);
            report_db(new_channel, SDB);
            fflush(new_channel);
            if (!dyn_loc_raw(dobj_sel[SDB],&(lobj[SDB][0]),&(pobj[SDB][0])))
                merge_flag |= EOF_SDB;
            else
                align_sub_objects(SDB, lobj[SDB][0],pobj[SDB][0],
                     lobj[SDB][1],pobj[SDB][1]);
        }
        else /* if (merge_flag == MATCH_FDB) */
        {    /* If the records match */
            fprintf(new_channel, "\n%*.*s %*.*s is present in both\n",
                lobj[SDB][0], lobj[SDB][0], pobj[SDB][0], 
                lobj[SDB][1], lobj[SDB][1], pobj[SDB][1]);
/*
 * Carry out a detail comparison of any sub-objects
 */
            fflush(new_channel);
            objcompare(new_channel); 
            fflush(new_channel);
            if (!dyn_loc_raw(dobj_sel[FDB],&(lobj[FDB][0]),&(pobj[FDB][0])))
                merge_flag |= EOF_FDB;
            else
                align_sub_objects(FDB, lobj[FDB][0],pobj[FDB][0],
                     lobj[FDB][1],pobj[FDB][1]);
            if (!dyn_loc_raw(dobj_sel[SDB],&(lobj[SDB][0]),&(pobj[SDB][0])))
                merge_flag |= EOF_SDB;
            else
                align_sub_objects(SDB, lobj[SDB][0],pobj[SDB][0],
                     lobj[SDB][1],pobj[SDB][1]);
        }
    }
    dyn_reset(dobj_sel[FDB]); /* Releases the parameter block */
    if (owner1 != owner2)
        dyn_reset(dobj_sel[SDB]); /* Releases the parameter block */
    clck = time(0);
    fprintf(new_channel, "\nEnd of report at %s", ctime(&clck));
    fclose(new_channel);
    return 1;
}
/*
 * Print out all details for a schema object
 */
static void report_db(fp, whch)
FILE * fp;
int whch;
{
    if (!strncmp(pobj[whch][0],"VIEW",lobj[whch][0]))
    {
/*
 * Print the view text
 */
        disp_print(fp, disp_view[whch]);
    }
    else
    if (!strncmp(pobj[whch][0],"TABLE",lobj[whch][0]))
    {
/*
 * Print the index columns, the constraint definitions and the triggers,
 * as well as the table columns. 
 */
        disp_print(fp, disp_tab[whch]);
        disp_print(fp, disp_ind[whch]);
        disp_print(fp, disp_con[whch]);
        disp_print(fp, disp_trg[whch]);
    }
    else
    if (!strncmp(pobj[whch][0],"FUNCTION",lobj[whch][0])
     || !strncmp(pobj[whch][0],"PACKAGE",lobj[whch][0])
     || !strncmp(pobj[whch][0],"PACKAGE BODY",lobj[whch][0])
     || !strncmp(pobj[whch][0],"PROCEDURE",lobj[whch][0]))
    {
/*
 * Print the source code. Is there a problem with packages and package
 * bodies?
 */
        disp_print(fp, disp_pls[whch]);
    }
    else
    if (!strncmp(pobj[whch][0],"SYNONYM",lobj[whch][0]))
    {
        disp_print(fp, disp_syn[whch]);
    }
    return;
}
/*
 * Compare and report on object differences
 */
static void objcompare(fp)
FILE *fp;
{
struct diffx_con * dp;

    if (!strncmp(pobj[FDB][0],"VIEW",lobj[FDB][0]))
    {
/*
 * Print the view text
 */
        dp = disp_compare(disp_view[FDB], disp_view[SDB]);
        diff_disp_rep(fp, disp_view[FDB], disp_view[SDB], dp, FULL_LIST);
        diffx_cleanup(dp);
    }
    else
    if (!strncmp(pobj[FDB][0],"TABLE",lobj[FDB][0]))
    {
/*
 * Print the index columns, the constraint definitions and the triggers,
 * as well as the table columns. 
 */
        dp = disp_compare(disp_tab[FDB], disp_tab[SDB]);
        diff_disp_rep(fp, disp_tab[FDB], disp_tab[SDB], dp, FULL_LIST);
        diffx_cleanup(dp);
        dp = disp_compare(disp_ind[FDB], disp_ind[SDB]);
        diff_disp_rep(fp, disp_ind[FDB], disp_ind[SDB], dp, FULL_LIST);
        diffx_cleanup(dp);
        dp = disp_compare(disp_con[FDB], disp_con[SDB]);
        diff_disp_rep(fp, disp_con[FDB], disp_con[SDB], dp, FULL_LIST);
        diffx_cleanup(dp);
        dp = disp_compare(disp_trg[FDB], disp_trg[SDB]);
        diff_disp_rep(fp, disp_trg[FDB], disp_trg[SDB], dp, FULL_LIST);
        diffx_cleanup(dp);
    }
    else
    if (!strncmp(pobj[FDB][0],"FUNCTION",lobj[FDB][0])
     || !strncmp(pobj[FDB][0],"PACKAGE",lobj[FDB][0])
     || !strncmp(pobj[FDB][0],"PACKAGE BODY",lobj[FDB][0])
     || !strncmp(pobj[FDB][0],"PROCEDURE",lobj[FDB][0]))
    {
/*
 * Print the source code. Is there a problem with packages and package
 * bodies?
 */
        dp = disp_compare(disp_pls[FDB], disp_pls[SDB]);
        diff_disp_rep(fp, disp_pls[FDB], dp, FULL_LIST);
        diffx_cleanup(dp);
    }
    else
    if (!strncmp(pobj[FDB][0],"SYNONYM",lobj[FDB][0]))
    {
        dp = disp_compare(disp_syn[FDB], disp_syn[SDB]);
        diff_disp_rep(fp, disp_syn[FDB], disp_syn[SDB], dp, FULL_LIST);
        diffx_cleanup(dp);
    }
    return;
}
