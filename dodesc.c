#include "tabdiff.h"
static char * desc_plsql="begin sys.dbms_describe.describe_procedure(:object_name,\
:res1,:res2,:overload, :position,:level,:argument,:datatype,:default,:in_out,\
:length,:precision, :scale,:radix,:spare); end;";
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
    set_long(80);
    cur_dict = new_dict( 20);
    dict_add(cur_dict,":object_name",ORA_LONG,  80);
    dict_add(cur_dict,":res1",ORA_LONG,  1);
    dict_add(cur_dict,":res2",ORA_LONG,  1);
    dict_add(cur_dict, ":overload", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":position", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":level", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":argument", ORA_LONG, 30);
    dict_add(cur_dict, ":datatype", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":default", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":in_out", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":length", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":precision", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":scale", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":radix", ORA_INTEGER, sizeof(long));
    dict_add(cur_dict, ":spare", ORA_INTEGER, sizeof(long));
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
        if (c < (d->bdp->C + 3))
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
void do_desc(con, obj_name)
struct sess_con * con;
char *obj_name;
{
static struct dyn_con * d;
char * x;
    if (d == (struct dyn_con *) NULL)
    {
        load_dict();
        d = dyn_init(con,3);
        d->statement = desc_plsql;
        prep_dml(d);
        make_arrs(d);
    }
#ifdef DEBUG
    fputs(obj_name, stderr);
    fputc('\n', stderr);
#endif
    d->cur_ind = 0;
    d->fld_ind = 0;
    add_bind(d, FIELD, strlen(obj_name), obj_name);
    add_bind(d, FIELD, 0, "");
    add_bind(d, FIELD, 0, "");
    exec_dml(d);
#ifdef DEBUG
    bind_print(stderr,d);
#endif
    return;
}
