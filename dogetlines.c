#include "tabdiff.h"
static char * getlines_plsql="begin sys.dbms_output.getlines(:lines,\
:numlines); end;";
/****************************************************************************
 * Load the bind variables into a dictionary
 */
static struct dict_con * cur_dict;
static void load_dict()
{
static int called = 0;
    if (called)
        return;
    else
        called = 1;
    set_long(255);
    cur_dict = new_dict( 2);
    dict_add(cur_dict,":lines",ORA_LONG,  255);
    dict_add(cur_dict,":numlines",ORA_INTEGER,  sizeof(long));
    (void) set_cur_dict(cur_dict);
    return;
}
static void make_arrs(d)
struct dyn_con * d;
{
/*
 * Scan the statement for : markers. Any found, stash them. Having found
 * them all, allocate the structure to hold them, and execute the bind
 * functions, using obndra().
 */
struct bv * anchor = (struct bv *) NULL;
struct bv * this_bv;
struct bv * found_bv;
register char * x, *x1;
int len;
int cnt =0;
char ** v;
char ** s;
short int **i;
int *l;
unsigned long int *p;
short int *c, *t;
unsigned short int **r, **o;
int row_len;                /* Width that needs to be allocated */
/*
 * Begin; find out how many distinct bind variables there are.
 */
    for (x = d->statement, row_len = 0; *x != '\0';)
    {
        switch(*x)
        {
        case ':':
            for (x1 = x + 1;
                *x1 == '_' || *x1 == '$'|| *x1 == '#' || isalnum(*x1);
                         x1++);
/*
 * Need to do some testing to see if we should ignore indicator variables
 */
            len = x1 - x;
            if (len > 1 && do_bv(&anchor,x, len, &this_bv))
            {
                if (cur_dict == (struct dict_con *) NULL
                 ||((found_bv = find_bv(cur_dict,x,len)) == (struct bv *) NULL))
                {
                    this_bv->dbtype = ORA_LONG;
                    this_bv->dsize = 80;
                    this_bv->dbsize = 80;
                    this_bv->scale = 0;
                    this_bv->prec = 0;
                    this_bv->nullok = 1;
                }
                else
                {
                    this_bv->dbtype = found_bv->dbtype;
                    this_bv->dsize = found_bv->dsize;
                    this_bv->dbsize = found_bv->dbsize;
                    this_bv->scale = found_bv->scale;
                    this_bv->prec = found_bv->prec;
                    this_bv->nullok = found_bv->nullok;
                }
                row_len += this_bv->dsize;
                cnt++;
            }
            x = x1;
            break;
        case '\'':
            for (;;)
            {
                x++;
                if (*x == '\0')
                    break; 
                if (*x == '\'')
                {
                    x++;
                    if (*x != '\'')
                        break;
                }
            }
            break;
        case '"':
            for (;;)
            {
                x++;
                if (*x == '\0')
                    break; 
                if (*x == '"')
                {
                    x++;
                    break;
                }
            }
            break;
        default:
            x++;
            break;
        }
    }
    sqlclu(d->bdp);
    if (cnt == 0)
    {
        d->bdp = (E2SQLDA *) NULL;
        return;
    }
    else
/*
 * Now allocate the items that can be allocated now
 */
        d->bdp = sqlald(cnt, 100, 1 + row_len/cnt);
/*
 *  Loop through the bind variables, storing them in association with
 *  the SQLDA and executing the bind function.
 */
    for (c = d->bdp->C,
         r = d->bdp->R,
         o = d->bdp->O,
         v = d->bdp->V,
         s = d->bdp->S,
         i = d->bdp->I,
         t = d->bdp->T,
         l = d->bdp->L,
         p = (unsigned long int *) d->bdp->P,
         x = d->bdp->base,
         this_bv = anchor;
             this_bv != (struct bv *) NULL;
                 c++, r++, o++, v++, s++, i++, t++, l++, p++)
    {
/*
 * First, deal with the stored variable name. It is not null terminated,
 * since it is embedded in the SQL statement.
 */
    short int *fo = (short int *) *o;

        *s = this_bv->bname;
        *c = this_bv->blen;
        *t = this_bv->dbtype;
        *l = this_bv->dsize;
        *v = x;
        anchor = this_bv;
        this_bv = this_bv->next;
        free(anchor);
        for (cnt = 0; cnt < d->bdp->arr; cnt++)
            *fo++ = (short int) *l;
/*
 * Now execute the bind itself
 */
        if (c > d->bdp->C)
        {
        if (!dbms_bind(d,
                *s,           /* The variable name                         */
                *c,           /* The length of the variable name           */  
                *v,           /* The address of the data area              */
                *l,           /* The data value length                     */
                *t,           /* The ORACLE Data Type                      */
                *i,           /* The indicator variables                   */
                *o,           /* The output lengths                        */
                *r))          /* The return codes                          */
            scarper(__FILE__,__LINE__,"variable bind failed");
        }
        else
        {
#ifdef OR9
        if (OCIBindByName(d->stmthp, &b, d->con->errhp, 
            *s,           /* The variable name                         */
            *c,           /* The length of the variable name           */  
            *v,           /* The address of the data area              */
            *l,           /* The data value length                     */
            *t,           /* The ORACLE Data Type                      */
            *i,           /* The indicator variables                   */
            *o,           /* The output lengths                        */
            *r,           /* The return codes                          */
            d->bdp->arr, /* Maximum PL/SQL Array size (0 for scalar)  */
            p,           /* The returned PL/SQL array sizes           */
            OCI_DEFAULT))
         {
        (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
             &(d->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
        d->con->ret_status = d->ret_status;
        (void) fprintf(stderr,
         "Error: %d Bind Name %*.*s Type:%d  Length:%d Routine:%d:%d\n%s\n",
                 d->ret_status,
                   c,c,s,t,l, isdigit(*(s+1)), atoi(s+1), d->statement);
            scarper(__FILE__,__LINE__,"Variable bind failed");
        }
#else
        if (obndra(&(d->cda),
            *s,           /* The variable name                         */
            *c,           /* The length of the variable name           */  
            *v,           /* The address of the data area              */
            *l,           /* The data value length                     */
            *t,           /* The ORACLE Data Type                      */
            -1,          /* Packed decimal scale (n/a)                       */
            *i,           /* The indicator variables                   */
            *o,           /* The output lengths                        */
            *r,           /* The return codes                          */
            d->bdp->arr, /* Maximum PL/SQL Array size (0 for scalar)  */
            p,           /* The returned PL/SQL array sizes           */
            (unsigned char *) 0,
                          /* COBOL only                                */
            -1,           /* COBOL only                                */
            -1))         /* COBOL only                                */
        {
            scarper(__FILE__,__LINE__,"Variable bind failed");
        }
#endif
        }
        x = x + (*l)*100;
    }
    if (x > (d->bdp->bound + 1))
    {
        fprintf(stderr, "Logic Error: Overrun in make_arrs: %s\n",
           (d->statement == NULL) ? "No statement" : d->statement);
        exit(1);
    }
    return;
}
/*
 * Hard coded routine to work round sqldrive's lack of support for PL/SQL
 * arrays
 */
void do_getlines(con, ofp)
struct sess_con * con;
FILE *ofp;
{
static struct dyn_con * d;
char * x;
    if (d == (struct dyn_con *) NULL)
    {
        load_dict();
        d = dyn_init(con,-7);
        d->statement = getlines_plsql;
        prep_dml(d);
        make_arrs(d);
    }
    d->cur_ind = 0;
    d->fld_ind = 0;
    add_bind(d, FIELD, 0, "");
    add_bind(d, FNUMBER, sizeof(int), &(d->bdp->arr));
    exec_dml(d);
    bind_print(ofp,d);
    return;
}
