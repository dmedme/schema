/***************************************************************************
 * e2schem_cmp - E2 Systems ORACLE Schema Comparator
 *
 * This file contains all the Windows bits. 32 bit only.
 */
static char * sccs_id="%W %D% %T% %E% %U%\n\
Copyright (c) E2 Systems Limited 1998";
#ifdef NT4
#include <windows.h>
#include <windowsx.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#include <errno.h>
#include "ansi.h"
#include "tabdiff.h"
#include "e2schem_cmp.h"
int proc_args ANSIARGS((int argc, char ** argv));
int parse_cmd_line ANSIARGS((char * szLine));
extern int getopt();
extern char *  optarg;
extern int optind;
extern int opterr;
#ifdef NT4
/*
 * Variables for Communication between Call Back procedures and main control 
 */
static struct sess_con * tmp_con;  /* Used for dialogue communication */
static char szCaption[128];
static OPENFILENAME ofn;
static char szFilterSpec [128] =                       /* file type filters */
                    "Text Files (*.TXT)\0*.TXT\0\0All Files (*.*)\0*.*\0";
static char szFileTitle[128] = "Output File for Schema Differences";
static char szFileName[256];
/***************************************************************************
 * Global data
 */
HINSTANCE hInst;      // Instance handle
HANDLE hAccelTable;   // Accelerator Table
HWND hwndMain;        //Main window handle
static char szName[] = "e2schem_cmp";
UINT MyHelpMessage;
WNDPROC OldProc;
WNDPROC OldProc1;
LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
BOOL WINAPI InitSQLListView(HWND hwndLV, struct dyn_con *pd);
BOOL WINAPI InitLVImLists(HWND hwndLV);
BOOL WINAPI InitSQLDALVColumns(HWND hwndLV, E2SQLDA *ps);
BOOL WINAPI InitSQLLVItems(HWND hwndLV, struct dyn_con *pd);
VOID WINAPI SetSQLLVText(LV_DISPINFO *pnmv);
BOOL WINAPI InitSQLTreeView(HWND hwndTV, struct dyn_con *pd);
BOOL WINAPI InitTVImLists(HWND hwndTV); 
BOOL WINAPI InitSQLDATVChildren(HWND hwndLV, E2SQLDA *ps);
BOOL WINAPI InitSQLTVItems(HWND hwndLV, struct dyn_con *pd);
VOID WINAPI SetSQLTVText(TV_DISPINFO *pnmv);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow);
static struct sess_con * do_oracle_session(HWND hwnd, LPCSTR sz);
/****************************************************************************
 *  PURPOSE: puts out a message box, using a format and a numeric and
 *  string argument.
 */
static void ShowError(lpTitle, lpFmt, lpStr, ulNum)
LPSTR lpTitle;
LPSTR lpFmt;
LPSTR lpStr;
LONG ulNum;
{
char buf[128];
    (void) wsprintf( (LPSTR) &buf[0],lpFmt, lpStr, ulNum);
    if (InSendMessage())
        ReplyMessage(TRUE);
    (void) MessageBox((HWND) NULL,
            (LPCSTR) &buf[0], lpTitle,
               MB_TASKMODAL|MB_ICONSTOP|MB_OK|MB_TOPMOST|MB_SETFOREGROUND);
    return;
}
/***************************************************************************
 * Junk needed by all users of e2sqllib.c
 */
void scarper(file_name,line,message)
char * file_name;
int line;
char * message;
{
    ShowError(file_name, "Unexpected Error, line %d error %d", (char *) line,
          errno);
    ShowError(file_name, message, (char *) NULL, 0);
    if (MessageBox((HWND) NULL,
            (LPCSTR) "Do you want to exit?", "e2schem_cmp error recovery",
               MB_TASKMODAL|MB_ICONSTOP|MB_YESNO|MB_TOPMOST|MB_SETFOREGROUND)
           == IDYES)
        PostQuitMessage(0);
    return;
}
/**************************************************************************
 * Functions for displaying the data tied to a dynamic statement control
 * block in a List View.
 *
 * The most general thing would be to have a function that accepted a SELECT
 * statement with bind variables, and which:
 * -  Looked up the bind variables in the dictionary (which would be extended
 *    with a description)
 * -  Threw up a window to prompt for the values
 * -  Executed the statement
 * -  Displayed the results, using the describe output for the headings
 *
 * Initially, the statements are pre-defined, and already executed. We simply
 * display a descriptor.
 * 
 * Create the list view.
 */
BOOL WINAPI InitSQLListView(HWND hwndLV, struct dyn_con *pd) 
{ 
    if (hwndLV == NULL) 
        return FALSE; 
/*
 * Call functions to initialize the 
 * image lists, add columns, and add some items. 
 */ 
    (void) InitLVImLists(hwndLV);
    if ( !InitSQLDALVColumns(hwndLV, pd->sdp))
    { 
        DestroyWindow(hwndLV); 
        ShowError(pd->statement,"List View Setup Failed - error: %d",
                   GetLastError());
        return FALSE; 
    } 
    (void) InitSQLLVItems(hwndLV, pd);
    return TRUE;
} 
/*
 * Add the icons to the list
 */
BOOL WINAPI InitLVImLists(HWND hwndLV) 
{ 
HICON hiconItem;        /* Icon for list view items              */
HIMAGELIST himlLarge;   /* Image list for icon view              */ 
HIMAGELIST himlSmall;   /* Image list for other views            */
/*
 * Create the full-sized and small icon image lists. We only use the
 * report view, so the other possibilities are not interesting. 
 */
    himlLarge = ImageList_Create(GetSystemMetrics(SM_CXICON), 
        GetSystemMetrics(SM_CYICON), TRUE, 3, 1); 
    himlSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON), 
        GetSystemMetrics(SM_CYSMICON), TRUE, 3, 1); 
/*
 * Add an icon to each image list. 
 */
    hiconItem = LoadImage(hInst, MAKEINTRESOURCE(ID_ICON_DEL),
                 IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR); 
    ImageList_AddIcon(himlLarge, hiconItem); 
    ImageList_AddIcon(himlSmall, hiconItem); 
    DeleteObject(hiconItem); 
    hiconItem = LoadImage(hInst, MAKEINTRESOURCE(ID_ICON_BOTH),
                 IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR); 
    ImageList_AddIcon(himlLarge, hiconItem); 
    ImageList_AddIcon(himlSmall, hiconItem); 
    DeleteObject(hiconItem); 
    hiconItem = LoadImage(hInst, MAKEINTRESOURCE(ID_ICON_INS),
                 IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR); 
    ImageList_AddIcon(himlLarge, hiconItem); 
    ImageList_AddIcon(himlSmall, hiconItem); 
    DeleteObject(hiconItem); 
/*
 * Assign the image lists to the list view control. 
 */
    ListView_SetImageList(hwndLV, himlLarge, LVSIL_NORMAL); 
    ListView_SetImageList(hwndLV, himlSmall, LVSIL_SMALL); 
    return TRUE; 
} 
/*
 * Add columns to a list view control.  
 * Returns TRUE if successful or FALSE otherwise. 
 */
BOOL WINAPI InitSQLDALVColumns(HWND hwndLV, E2SQLDA *ps) 
{ 
char buf[256];
LV_COLUMN lvc; 
int iCol; 
/*
 * Initialize the LV_COLUMN structure. 
 */
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
    lvc.fmt = LVCFMT_LEFT; 
    lvc.cx = 100; 
    lvc.pszText = &buf[0]; 
/*
 * Add the columns. These come from the list in the SQLDA.
 */
    for (iCol = 0; iCol < ps->F; iCol++)
    { 
        lvc.iSubItem = iCol; 
        memcpy(&buf[0], ps->S[iCol], ps->C[iCol]);
        buf[ps->C[iCol]] = '\0';
        if (ListView_InsertColumn(hwndLV, iCol, &lvc) == -1) 
        {
            ShowError(&buf[0],
                   "ListView_InsertColumn Failed - error: %d",
                   GetLastError());
            return FALSE; 
        }
        ListView_SetColumnWidth(hwndLV, iCol, LVSCW_AUTOSIZE_USEHEADER);
    } 
    return TRUE; 
} 
/* 
 * We fill in an LV_ITEM structure and add list view items by
 * using the LVM_INSERTITEM message.
 *
 * We save space by using the LPSTR_TEXTCALLBACK value for the pszText member
 * of the LV_ITEM structure. Specifying this value causes the control to send
 * an LVN_GETDISPINFO notification message to its owner window whenever it
 * needs to display the item. The item index information identifies which
 * element in the SQLDA is to be displayed. We save a pointer to the SQLDA as
 * item data.
 * 
 * The following adds the items and subitems to a list view.  
 * Returns TRUE if successful or FALSE otherwise. 
 * hwndLV - handle of the list view control 
 * pd - pointer to dynamic statement control block
 */
BOOL WINAPI InitSQLLVItems(HWND hwndLV, struct dyn_con *pd) 
{ 
int iItem; 
LV_ITEM lvi; 
/*
 * Initialize LV_ITEM members that are common to all items. 
 */
    lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE; 
    lvi.state = 0; 
    lvi.stateMask = 0; 
    lvi.iImage = 0;                     /* Image list index; our icon */ 
    lvi.lParam = (LPARAM) pd;           /* The dynamic control block  */
    iItem = pd->sdp->F * pd->to_do;     /* The total number to add    */
    if (!iItem)
    {
        lvi.pszText = "No Data Found";   /* An error message          */
        lvi.iItem = 0; 
        lvi.iSubItem = 0; 
        ListView_InsertItem(hwndLV, &lvi); 
    }
    else
    {
        lvi.pszText = LPSTR_TEXTCALLBACK;   /* We maintain this ourselves */
        ListView_SetItemCount(hwndLV, iItem);
/*
 * For each row in the block add the item and sub-items. 
 */
        for (lvi.iItem = 0; lvi.iItem <   iItem; lvi.iItem++)
            for (lvi.iSubItem = 0; lvi.iSubItem < pd->sdp->F; lvi.iSubItem++)
                ListView_InsertItem(hwndLV, &lvi); 
    }
    return TRUE; 
} 
/*
 * The following example shows the application-defined functions that the
 * window procedure uses to process list view notification messages.
 *
 * Process the LVN_GETDISPINFO notification message. 
 * - pnmv - value of lParam (points to an LV_DISPINFO structure) 
 */
VOID WINAPI SetSQLLVText(LV_DISPINFO *pnmv) 
{ 
/*
 * Provide the item or subitem's text, if requested. 
 */
    if (pnmv->item.mask & LVIF_TEXT)
    { 
    struct dyn_con  *pd = (struct dyn_con *) (pnmv->item.lParam); 
    int len;
    char *psz;

        col_render(pd, pd->sdp, pnmv->item.iItem, pnmv->item.iSubItem,
                       &len, &psz);
        pnmv->item.pszText = psz;
    } 
    return;
} 
/*
 * Process the LVN_ENDLABELEDIT notification message. 
 * - Returns TRUE if the label is changed or FALSE otherwise. 
 * -  pnmv - value of lParam (points to an LV_DISPINFO structure) 
 */
BOOL GetSQLLVText(LV_DISPINFO *pnmv) 
{ 
struct dyn_con *pd; 
/*
 * The item is -1 if editing is being canceled. 
 */
    if (pnmv->item.iItem == -1) 
        return FALSE; 
/*
 * Copy the new text to the application-defined structure, 
 */
    pd = (struct dyn_con *) (pnmv->item.lParam); 
    (void) col_patch(pd->sdp, pnmv->item.iItem, pnmv->item.iSubItem,
                  strlen(pnmv->item.pszText), pnmv->item.pszText);
/*
 *  No need to set the item text, because it is a callback item. 
 */
    return TRUE; 
} 
/*
 * A list view control notifies its parent window of events by sending a
 * WM_NOTIFY message. The wParam parameter is the identifier of the list view
 * control, and the lParam parameter is the address of an NMHDR structure (or
 * to a larger structure that has an NMHDR structure as its first member). The
 * example in this section processes the LVN_GETDISPINFO, LVN_ENDLABELEDIT,
 * and LVN_COLUMNCLICK notification messages.
 *
 * A list view control sends the LVN_GETDISPINFO notification message to
 * retrieve information about an item or subitem from the parent window. This
 * notification is sent, for example, when an item with the LPSTR_TEXTCALLBACK
 * value needs to be displayed.
 *
 * A list view control sends the LVN_ENDLABELEDIT notification message when
 * the user completes or cancels editing of an item's label. This notification
 * is only sent if the list view control has the LVS_EDITLABELS window style.
 * If editing is being canceled, the parent window typically does nothing. If
 * editing is being completed, the parent window should set the item label to
 * the new text, unless the item label is LPSTR_TEXTCALLBACK. In that case, the
 * parent window should simply update the application-defined data it maintains
 * for the list item.
 *
 * If the user clicks a column header in report view, a list view control sends
 * the LVN_COLUMNCLICK notification message. Typically, an application sorts a
 * list view by the specified column when this clicking occurs. To sort, use
 * the LVM_SORTITEMS message, specifying an application-defined comparison
 * function.
 *
 * Standard Windows Dialog Window Procedures. Needed so that notifies can
 * be handled correctly.
 */
LRESULT __stdcall ListWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_NOTIFY:  
/*
 * Branch depending on the specific notification message. 
 */
#ifdef DEBUG
        ShowError("Received WM_NOTIFY", "wParam %x lParam %x",
                          wParam, lParam);
#endif
/*
 * Branch depending on the specific notification message. 
 */
        switch (((LPNMHDR) lParam)->code)
        { 
/*
 * Process LVN_GETDISPINFO to supply information about callback items. 
 */
        case LVN_GETDISPINFO: 
            SetSQLLVText((LV_DISPINFO *) lParam); 
            break; 
/*
 * Process LVN_ENDLABELEDIT to change stored values after 
 * in-place editing. 
 */
        case LVN_ENDLABELEDIT: 
            return GetSQLLVText( (LV_DISPINFO *) lParam ); 
            return TRUE; 
        default:
#ifdef DEBUG
            ShowError("WM_NOTIFY Message", "Code %x Window %x",
                 ((int) ((LPNMHDR) lParam)->code),
                        ((((LPNMHDR) lParam)->hwndFrom)));
#endif
            break; 
        } 
        break; 
    default:
        return DefDlgProc(hwnd,msg,wParam,lParam);
    }
    return 0;
}
LRESULT __stdcall TreeWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_NOTIFY:  
/*
 * Branch depending on the specific notification message. 
 */
#ifdef DEBUG
        ShowError("Received WM_NOTIFY", "wParam %x lParam %x",
                          wParam, lParam);
#endif
/*
 * Branch depending on the specific notification message. 
 */
        switch (((LPNMHDR) lParam)->code)
        { 
/*
 * Process TVN_GETDISPINFO to supply information about callback items. 
 */
        case TVN_GETDISPINFO: 
            SetSQLTVText((TV_DISPINFO *) lParam); 
            break; 
/*
 * Process TVN_ENDLABELEDIT to change stored values after 
 * in-place editing. 
 */
        case TVN_ENDLABELEDIT: 
            return GetSQLTVText( (TV_DISPINFO *) lParam ); 
            return TRUE; 
        default:
#ifdef DEBUG
            ShowError("WM_NOTIFY Message", "Code %x Window %x",
                 ((int) ((LPNMHDR) lParam)->code),
                        ((((LPNMHDR) lParam)->hwndFrom)));
#endif
            break; 
        } 
        break; 
    default:
        return DefDlgProc(hwnd,msg,wParam,lParam);
    }
    return 0;
}
/**************************************************************************
 * Functions to manipulate a Tree View control, used to provide the list
 * of schema objects.
 */
BOOL WINAPI InitSQLTreeView(HWND hwndTV, struct dyn_con *pd) 
{ 
    if (hwndTV == NULL) 
        return FALSE; 
/*
 * Call functions to initialize the 
 * image lists, add columns, and add some items. 
 */ 
    (void) InitTVImLists(hwndTV);
    (void) InitSQLTVItems(hwndTV, pd);
    return TRUE;
} 
/*
 * Add the icons to the list
 */
BOOL WINAPI InitTVImLists(HWND hwndLV) 
{ 
HICON hiconItem;        /* Icon for list view items              */
HIMAGELIST himlLarge;   /* Image list for icon view              */ 
/*
 * Create the full-sized and small icon image lists. We only use the
 * report view, so the other possibilities are not interesting. 
 */
    himlLarge = ImageList_Create(GetSystemMetrics(SM_CXICON), 
        GetSystemMetrics(SM_CYICON), TRUE, 3, 1); 
/*
 * Add an icon to each image list. 
 */
    hiconItem = LoadImage(hInst, MAKEINTRESOURCE(ID_ICON_DEL),
                 IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR); 
    ImageList_AddIcon(himlLarge, hiconItem); 
    DeleteObject(hiconItem); 
    hiconItem = LoadImage(hInst, MAKEINTRESOURCE(ID_ICON_BOTH),
                 IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR); 
    ImageList_AddIcon(himlLarge, hiconItem); 
    DeleteObject(hiconItem); 
    hiconItem = LoadImage(hInst, MAKEINTRESOURCE(ID_ICON_INS),
                 IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR); 
    ImageList_AddIcon(himlLarge, hiconItem); 
    DeleteObject(hiconItem); 
/*
 * Assign the image lists to the list view control. 
 */
    TreeView_SetImageList(hwndLV, himlLarge, TVSIL_NORMAL); 
    return TRUE; 
} 
/******************************************************************************
 * Add children to the TreeView.
 *
 * These function as buttons for the sub-structure of an object.
 *
 * The root is the list of objects, and their presence in the two schemas that
 * are being compared.
 *
 * Objects may have direct descendants (eg. procedures and lines of code) or
 * a level of categories (eg. triggers, indexes, columns), which themselves
 * have further descendants  (eg. the indexes, which have columns). We actually
 * have a clear maximum number of hierarchy levels, but the numbers of levels
 * vary according to the category of the corresponding root object.
 *
 * All the lines in an E2SQLDA would become the children. If there is only one
 * column, this can become a further tree element, but multiple columns imply
 * a ListView instead. The program needs a data structure that reflects the
 * actual data displayed.
 */
BOOL WINAPI InitSQLDATVChildren(HWND hwndTV, E2SQLDA *ps) 
{ 
LPSTR lpszItem;
int nLevel;
{ 
TV_ITEM tvi; 
TV_INSERTSTRUCT tvins; 
static HTREEITEM hPrev = (HTREEITEM) TVI_FIRST; 
static HTREEITEM hPrevRootItem = NULL; 
static HTREEITEM hPrevLev2Item = NULL; 
HTREEITEM hti; 

 
    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM; 
 
    // Set the text of the item. 
    tvi.pszText = lpszItem; 
    tvi.cchTextMax = lstrlen(lpszItem); 
 
    // Assume the item is not a parent item, so give it a 
    // document image. Zero based index into the image list. 
/*    tvi.iImage = g_nDocument; 
    tvi.iSelectedImage = g_nDocument; 
*/ 
    // Save the heading level in the item's application-defined 
    // data area. 
    tvi.lParam = (LPARAM) nLevel; 
 
    tvins.item = tvi; 
    tvins.hInsertAfter = hPrev; 
 
    // Set the parent item based on the specified level. 
    if (nLevel == 1) 
        tvins.hParent = TVI_ROOT; 
    else if (nLevel == 2) 
        tvins.hParent = hPrevRootItem; 
    else 
        tvins.hParent = hPrevLev2Item; 
 
    // Add the item to the tree view control. 
    hPrev = (HTREEITEM) SendMessage(hwndTV, TVM_INSERTITEM, 0, 
         (LPARAM) (LPTV_INSERTSTRUCT) &tvins); 
 
    // Save the handle to the item. 
    if (nLevel == 1) 
        hPrevRootItem = hPrev; 
    else if (nLevel == 2) 
        hPrevLev2Item = hPrev; 
 
    // The new item is a child item. Give the parent item a 
    // closed folder bitmap to indicate it now has child items. 
    if (nLevel > 1) { 
        hti = TreeView_GetParent(hwndTV, hPrev); 
        tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE; 
        tvi.hItem = hti; 
/*        tvi.iImage = g_nClosed;  
        tvi.iSelectedImage = g_nClosed;  */
        TreeView_SetItem(hwndTV, &tvi); 
    } 
 
/*    return hPrev;  */
} 
 
    return TRUE; 
} 
/* 
 * We fill in an TV_ITEM structure and add tree view items by
 * using the TVM_INSERTITEM message.
 *
 * We save space by using the LPSTR_TEXTCALLBACK value for the pszText member
 * of the TV_ITEM structure. Specifying this value causes the control to send
 * an TVN_GETDISPINFO notification message to its owner window whenever it
 * needs to display the item. The item index information identifies which
 * element in the SQLDA is to be displayed. We save a pointer to the SQLDA as
 * item data.
 * 
 * The following adds the items and subitems to a tree view.  
 * Returns TRUE if successful or FALSE otherwise. 
 * hwndTV - handle of the tree view control 
 * pd - pointer to dynamic statement control block
 */
BOOL WINAPI InitSQLTVItems(HWND hwndTV, struct dyn_con *pd) 
{ 
int iItem; 
TV_ITEM lvi; 
/*
 * Initialize TV_ITEM members that are common to all items. 
 */
    lvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_PARAM | TVIF_STATE; 
    lvi.state = 0; 
    lvi.stateMask = 0; 
    lvi.iImage = 0;                     /* Image list index; our icon */ 
    lvi.lParam = (LPARAM) pd;           /* The dynamic control block  */
    iItem = pd->sdp->F * pd->to_do;     /* The total number to add    */
    if (!iItem)
    {
        lvi.pszText = "No Data Found";   /* An error message          */
        TreeView_InsertItem(hwndTV, &lvi); 
    }
    else
    {
        lvi.pszText = LPSTR_TEXTCALLBACK;   /* We maintain this ourselves */
/*
        TreeView_SetItemCount(hwndTV, iItem);
 * For each row in the block add the item and sub-items. 
BOOL WINAPI InitSQLDATVChildren(HWND hwndTV, E2SQLDA *ps) 
        for (lvi.iItem = 0; lvi.iItem <   iItem; lvi.iItem++)
            for (lvi.iSubItem = 0; lvi.iSubItem < pd->sdp->F; lvi.iSubItem++)
                TreeView_InsertItem(hwndTV, &lvi); 
 */
    }
    return TRUE; 
} 
/*
 * The following example shows the application-defined functions that the
 * window procedure uses to process list view notification messages.
 *
 * Process the TVN_GETDISPINFO notification message. 
 * - pnmv - value of lParam (points to an TV_DISPINFO structure) 
 */
VOID WINAPI SetSQLTVText(TV_DISPINFO *pnmv) 
{ 
/*
 * Provide the item or subitem's text, if requested. 
 */
    if (pnmv->item.mask & TVIF_TEXT)
    { 
    struct dyn_con  *pd = (struct dyn_con *) (pnmv->item.lParam); 
    int len;
    char *psz;

/*        col_render(pd, pd->sdp, pnmv->item.iItem, pnmv->item.iSubItem,
                       &len, &psz);
        pnmv->item.pszText = psz;
*/
    } 
    return;
} 
/*
 * Process the TVN_ENDLABELEDIT notification message. 
 * - Returns TRUE if the label is changed or FALSE otherwise. 
 * -  pnmv - value of lParam (points to an TV_DISPINFO structure) 
 */
BOOL GetSQLTVText(TV_DISPINFO *pnmv) 
{ 
struct dyn_con *pd; 
/*
 * The item is -1 if editing is being canceled. 
 *o
    if (pnmv->item.iItem == -1) 
        return FALSE; 
o*
 * Copy the new text to the application-defined structure, 
 *o
    pd = (struct dyn_con *) (pnmv->item.lParam); 
    (void) col_patch(pd->sdp, pnmv->item.iItem, pnmv->item.iSubItem,
                  strlen(pnmv->item.pszText), pnmv->item.pszText);
o*
 *  No need to set the item text, because it is a callback item. 
 */
    return TRUE; 
} 
/**************************************************************************
 * Callback function for the routine that logs a session on to ORACLE
 */
BOOL CALLBACK
DlgOracleSession (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
char szOrUser[32];
char szOrPassword[32];
char szOrTNS[256];
char szLogin[378];
    if (InSendMessage())
        ReplyMessage(TRUE);

    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowText(hDlg, szCaption);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            (void) GetDlgItemText(hDlg, ID_ETORUSER,szOrUser,
                    sizeof(szOrUser) - 1);
            (void) GetDlgItemText(hDlg, ID_ETORPASS,szOrPassword,
                    sizeof(szOrPassword) - 1);
            (void) GetDlgItemText(hDlg, ID_ETORTNS,szOrTNS,
                    sizeof(szOrTNS) - 1);
#ifdef DEBUG
            ShowError(szName, "Length of szOrUser: %u",
                     strlen(szOrUser)); 
            ShowError(szName, "Length of szOrPassword: %u",
                     strlen(szOrPassword)); 
            ShowError(szName, "Length of szOrTNS: %u",
                     strlen(szOrTNS)); 
#endif
            wsprintf(szLogin,"%s/%s@%s",szOrUser,szOrPassword,szOrTNS);
            if ((tmp_con = dyn_connect(szLogin, "e2schem_cmp"))
                             == (struct sess_con *) NULL)
            {
                (void) ShowError(szOrUser, "Failed to log on to database %s\n",
                       (LONG) &szOrTNS);
            }
            else
            {
                wsprintf(tmp_con->description,"%s@%s",szOrUser, szOrTNS);
                EndDialog(hDlg, TRUE);
            }
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, FALSE);
            return TRUE;

        case ID_HELP:
DoHelp:
#if 0
                    bHelpActive=WinHelp(hDlg, szHelpFile, HELP_CONTEXT,
                                    IDH_DLG_ORASESSION);
                    if (!bHelpActive)
                        MessageBox(hDlg, "Failed to start help", szName, MB_OK);
#else
            MessageBox(hDlg, "Insert your call to WinHelp() here.",
                        szName,  MB_OK);
#endif
            break;
        }
        break;
    default:
        break;
        if (msg==MyHelpMessage)     // Context sensitive help msg.
            goto DoHelp;
    }
    return FALSE;
}

/*******************************************************************************
 * Standard windows flannel
 */
static BOOL InitApplication(void)
{
WNDCLASSEX wc;      /* A window class structure. */

    if (!GetClassInfoEx(hInst, WC_DIALOG, &wc))
    {
        ShowError("InitApplication",
             "Failed to get DIALOGBOX class information: Error %u",
              GetLastError());
        return FALSE;
    }
/*
 * Change the bits that need to be altered
 */
    wc.lpszMenuName = NULL;
    wc.hInstance = hInst;
    wc.cbSize = sizeof(wc);
    wc.hIcon  = LoadImage(hInst, MAKEINTRESOURCE(IDI_APPLICATION), IMAGE_ICON,
                           0, 0, LR_DEFAULTCOLOR); 
    wc.lpfnWndProc = (WNDPROC) TreeWndProc;
    wc.lpszClassName = "E2DIFF_TREE";
/*
 * Now register the Tree window dialogue class for use.
 */
    if (!RegisterClassEx (&wc))
        ShowError(szName, 
                       (LPSTR) "Failed to Register Class, Error:%d",
                GetLastError());
    wc.lpfnWndProc = (WNDPROC) ListWndProc;
    wc.lpszClassName = "E2DIFF_LIST";
/*
 * Now register the View window dialog class for use.
 */
    if (!RegisterClassEx (&wc))
        ShowError(szName, 
                       (LPSTR) "Failed to Register Class, Error:%d",
                GetLastError());
/*
 * Change the bits that need to be altered
 */
    wc.lpfnWndProc = (WNDPROC)MainWndProc;
    wc.lpszClassName = "E2_SCHEMA_CMP";
    wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
/*
 * Now register the Main window class for use.
 */
    if (!RegisterClassEx (&wc))
    {
        ShowError(szName, 
                       (LPSTR) "Failed to Register Class, Error:%d",
                GetLastError());
        return 0;
    }
/*
 * Set up the output file template
 */
    ofn.lStructSize       = sizeof(OPENFILENAME);
    ofn.lpstrFilter   = szFilterSpec;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter    = 0;
    ofn.nFilterIndex      = 1;
    ofn.lpstrFile         = szFileName;
    ofn.nMaxFile      = 128;
    ofn.lpstrInitialDir   = NULL;
    ofn.lpstrFileTitle    = szFileTitle;
    ofn.nMaxFileTitle     = 128;
    ofn.lpstrTitle        = NULL;
    ofn.lpstrDefExt       = "txt";
    ofn.Flags             = 0;
    return 1;
}
/*****************************************************************************
 * The About dialogue
 */
BOOL WINAPI AboutDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg) {
        case WM_CLOSE:
            EndDialog(hwnd,0);
            return 1;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    EndDialog(hwnd,1);
                    return 1;
            }
            break;
    }
    return 0;
}
/*****************************************************************************
 * Standard Windows Main Window Procedure
 */
LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
/*
 * Dismantle any ORACLE sessions
 */
        PostQuitMessage(0);
        break;
    default:
        return DefDlgProc(hwnd,msg,wParam,lParam);
    }
    return 0;
}
/*
 * Standard windows event loop
 */
static int do_message_loop()
{
MSG msg;
    while (GetMessage(&msg,NULL,0,0))
    {
        if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
#ifdef LCC
    (void) fclose(stderr);
#endif
#ifdef DEBUG
    ShowError((LPSTR) "do_message_loop",
                       (LPSTR) "Exit Message %u",
                    msg.message);
#ifdef LCC
   (void) fclose(stdout);
#endif
#endif
    return msg.wParam;
}
/*
 * Procedure to patch in the desired window text and implement tabbing
 * between input fields that normally capture the TAB key.
 */
LRESULT CALLBACK TextProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
static LPCSTR szText[5];
static HWND hwndSav[5];
static int seen_cnt;
static int shift_seen;
int i;
static HFONT hfnt;
/*
 * Handle up to 5 windows
 */
    for (i = 0; i < seen_cnt; i++)
        if (hwnd == hwndSav[i])
            break; 
    if (i >= seen_cnt && i < 4)
    {
        seen_cnt++;
        hwndSav[i] = hwnd;
    }
    switch (msg)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTTAB;
    case WM_KEYDOWN:
        if (wParam == VK_SHIFT )
            shift_seen = 1;
        break;
    case WM_CHAR:
        if (wParam == VK_TAB )
        {
            if (!shift_seen &&
                 (szText[i] == (LPCSTR) NULL || szText[i][0] == '\0'))
                SendNotifyMessage(GetParent(hwnd), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(hwnd),CBN_DROPDOWN),
                     (LPARAM) hwnd);
            else
                SetFocus(GetNextDlgTabItem(GetParent(hwnd), hwnd, shift_seen));
            return FALSE;
        }
        else
        if (szText[i] == (LPCSTR) NULL || szText[i][0] == '\0')
        {
            SendNotifyMessage(GetParent(hwnd), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(hwnd),CBN_DROPDOWN),
                     (LPARAM) hwnd);
            return FALSE;
        }
        break;
    case WM_KEYUP:
        if (wParam == VK_SHIFT )
            shift_seen = 0;
        break;
    case WM_SETTEXT:
        szText[i] = (LPCSTR) lParam;
        InvalidateRect(hwnd,NULL,FALSE);
        return TRUE;
    case WM_PAINT:
    {
    RECT rc;
    SIZE size;
    HFONT hOldFont;
    HDC hdc = GetDC(hwnd);
    COLORREF crf = 0L;                /* Black */
    COLORREF crfold;
    int oldmode;
        if (szText[i] == (char *) NULL)
            break;
        SetTextAlign(hdc, TA_LEFT|TA_TOP|TA_NOUPDATECP);
        oldmode = GetBkMode(hwnd);
        crfold = GetTextColor(hwnd);
        if (hfnt == NULL)
           hfnt = CreateFont(
              -MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72),
                               /* Height of 8 point font         */
              0,               /* Width; let the system choose   */
              0,               /* Angle (escapement) for individual character */
              0,               /* Angle (orientation) for string */
              0,               /* Normal weight                  */
              FALSE,           /* Not Italic                     */
              FALSE,           /* Not Underlines                 */
              FALSE,           /* Not Stricken Out               */
              ANSI_CHARSET,    /* Don't want Hebrew or Greek!    */
              OUT_DEVICE_PRECIS,
                               /* Get the best possible match    */
              CLIP_DEFAULT_PRECIS,
                               /* Default Clipping               */
              DEFAULT_QUALITY,
                               /* Default Quality                */
              FF_SWISS|VARIABLE_PITCH,
                               /* The sort of font we want       */
              NULL);           /* I am not asking for a particular name */
        if (hOldFont = SelectObject(hdc, hfnt))
        { 
            GetTextExtentPoint32(hdc, szText[i], strlen(szText[i]), &size);
            rc.left = 5;
            rc.top = 4;
            rc.right = 5 + size.cx;
            rc.bottom = 4 + size.cy;
            SetTextColor(hwnd, crf);
            SetBkMode(hwnd, TRANSPARENT);
            ExtTextOut(hdc, 5, 4, 0,
                        &rc, szText[i],  strlen(szText[i]),
                                  NULL); 
            SelectObject(hdc, hOldFont);
            SetTextColor(hwnd, crfold);
            SetBkMode(hwnd, oldmode);
        } 
        ReleaseDC(hwnd, hdc);
        ValidateRect(hwnd, &rc);
        break;
    }
    case WM_KILLFOCUS:
        SendNotifyMessage(GetParent(hwnd), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(hwnd),CBN_CLOSEUP),
                     (LPARAM) hwnd);
        shift_seen = 0;
        return FALSE;
    case WM_DESTROY:
        if (hfnt != NULL)
            DeleteObject(hfnt);
    }
    return CallWindowProc(OldProc, hwnd, msg,wParam,lParam);
}
/*
 * Procedure to implement tabbing
 * between input fields that normally capture the TAB key.
 */
LRESULT CALLBACK TabProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
static int shift_seen;
    switch (msg)
    {
    case WM_KILLFOCUS:
        shift_seen = 0;
        break;
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTTAB;
    case WM_KEYDOWN:
        if (wParam == VK_SHIFT )
            shift_seen = 1;
        break;
    case WM_CHAR:
        if (wParam == VK_TAB )
        {
#ifdef DEBUG
            fprintf(stderr, "hwnd:%x ->hwnd:%x ->hwnd:%x next1:%x next2:%x shift_seen:%d\n",
                     hwnd, GetParent(hwnd), GetParent(GetParent(hwnd)),
                GetNextDlgTabItem(GetParent(GetParent(hwnd)),
                            GetParent(hwnd), shift_seen),
                GetNextDlgTabItem(GetParent(hwnd),
                            hwnd, shift_seen), shift_seen);
#endif
            SetFocus(GetNextDlgTabItem(GetParent(GetParent(hwnd)),
                            GetParent(hwnd), shift_seen));
            return FALSE;
        }
        else
            break;
    case WM_KEYUP:
        if (wParam == VK_SHIFT )
            shift_seen = 0;
        break;
    }
    return CallWindowProc(OldProc1, hwnd, msg,wParam,lParam);
}
/*
 * The assumption made by the following routines is that all the Combo boxes
 * have the same window procedure.
 */
HWND PASCAL E2ComboSubClass(HWND hwndCtl, WNDPROC * PriorProc)
{
POINT pt;
HWND hCBE;
    pt.x = 15;
    pt.y = 15;
    if ((hCBE = ChildWindowFromPoint(hwndCtl,pt)) != NULL)
    {
        if (TextProc != (WNDPROC) GetWindowLong(hCBE, GWL_WNDPROC))
            *PriorProc = (WNDPROC) SetWindowLong(hCBE, GWL_WNDPROC,
                                      (LONG) TextProc);
    }
    return hCBE;
}
/*
 * The assumption made by the following routines is that all the Combo boxes
 * have the same window procedure.
 */
HWND PASCAL E2ComboEditSubClass(HWND hwndCtl, WNDPROC * PriorProc)
{
POINT pt;
HWND hCBE;
    pt.x = 15;
    pt.y = 15;
    if ((hCBE = ChildWindowFromPoint(hwndCtl,pt)) != NULL)
    {
        if (TabProc != (WNDPROC) GetWindowLong(hCBE, GWL_WNDPROC))
            *PriorProc = (WNDPROC) SetWindowLong(hCBE, GWL_WNDPROC,
                                      (LONG) TabProc);
    }
    return hCBE;
}
BOOL FAR PASCAL DlgE2SchemaCmp(HWND hDlg, UINT msg,
       WPARAM wParam, LPARAM lParam)
{
static HWND hIDOK;
static HWND hCBS1;
static HWND hCBU1;
static HWND hCBS2;
static HWND hCBU2;
static HWND hCBOF;
static HWND hIDCANCEL;
static HWND hStatus;
static struct sess_con * sess1;
static struct sess_con * sess2;
static char szUser1[64];
static char szUser2[64];
UINT uiMsg;
HWND hwndCtl;
WORD id;
HCURSOR hPrev, hHour;
    hHour = LoadCursor(NULL, IDC_WAIT);

    switch (msg)
    {
    case WM_INITDIALOG:
        hIDOK=GetDlgItem(hDlg, IDOK);
        EnableWindow(hIDOK, FALSE); /* Disable OK until all there */
        hCBS1=GetDlgItem(hDlg, ID_CBFRST_SIGNON);
        (void) E2ComboSubClass(hCBS1, &OldProc);
        hCBU1=GetDlgItem(hDlg, ID_CBFRST_USER);
        (void) E2ComboEditSubClass(hCBU1, &OldProc1);
        EnableWindow(hCBU1, FALSE); /* Disable user until session there */
        hCBS2=GetDlgItem(hDlg, ID_CBSCND_SIGNON);
        (void) E2ComboSubClass(hCBS2, &OldProc);
        hCBU2=GetDlgItem(hDlg, ID_CBSCND_USER);
        (void) E2ComboEditSubClass(hCBU2, &OldProc1);
        hIDCANCEL=GetDlgItem(hDlg, IDCANCEL);
        EnableWindow(hCBU2, FALSE); /* Disable user until session there */
        hCBOF=GetDlgItem(hDlg, ID_CBOFILE);
        (void) E2ComboSubClass(hCBOF, &OldProc);
        hStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE,
                  "Fill in the Fields and Press OK", hDlg, IDW_STATUS);
        return TRUE;
    case WM_COMMAND:
/*
 * Window command processing.
 */
        uiMsg = HIWORD(wParam);
        hwndCtl = (HWND) lParam;
        id = LOWORD(wParam);
        switch(id)
        {
        case ID_CBFRST_SIGNON:
            if (uiMsg == CBN_DROPDOWN)
            if ((sess1 = do_oracle_session(hDlg, "First ORACLE Session"))
                   != (struct sess_con *) NULL)
            {
                SetWindowText(hwndCtl, sess1->description);
                EnableWindow(hCBU1, TRUE);     /* Enable users               */
                EnableWindow(hwndCtl, FALSE);  /* Cannot now change          */
                SetFocus(hCBU1);
            }
            break;
        case ID_CBFRST_USER:
            if (uiMsg == CBN_KILLFOCUS)
            {
                GetDlgItemText(hDlg, ID_CBFRST_USER, szUser1, sizeof(szUser1));
            }
            break;
        case ID_CBSCND_SIGNON:
            if (uiMsg == CBN_DROPDOWN)
            if ((sess2 = do_oracle_session(hDlg, "Second ORACLE Session"))
                   != (struct sess_con *) NULL)
            {
                SetWindowText(hwndCtl, sess2->description);
                EnableWindow(hwndCtl, FALSE); /* Cannot now change         */
                EnableWindow(hCBU2, TRUE);
                SetFocus(hCBU2);
            }
            break;
        case ID_CBSCND_USER:
            if (uiMsg == CBN_KILLFOCUS)
            {
                GetDlgItemText(hDlg, ID_CBSCND_USER, szUser2, sizeof(szUser2));
            }
            break;
        case ID_CBOFILE:
        case IDM_OPEN:
        case IDM_NEW:
            if (id == ID_CBOFILE && uiMsg != CBN_DROPDOWN)
                break;
            ofn.hwndOwner     = hDlg;
            if (GetOpenFileName ((LPOPENFILENAME)&ofn))
            {
                SetWindowText(hwndCtl, szFileName);
                SetFocus(GetNextDlgTabItem(hDlg, hwndCtl, FALSE));
            }
            break;
        case IDM_ABOUT:
            (void) DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUT),
                    hDlg,AboutDlg);
            break;
    
        case IDOK:
/*
 * Actually do the comparison
 */
            SetWindowText(hStatus, "Processing Request");
            hPrev = GetCursor();
            SetCursor(hHour);
            (void) schm_cmp_main(sess1, sess2, szUser1, szUser2, szFileName);
            SetWindowText(hStatus, "Fill in the Fields and Press OK");
            SetCursor(hPrev);
            break;
        case IDM_EXIT:
        case IDCANCEL:
            if (sess1 != (struct sess_con *) NULL)
                dyn_disconnect(sess1);
            if (sess2 != (struct sess_con *) NULL)
                dyn_disconnect(sess2);
            EndDialog(hDlg, FALSE);
            PostQuitMessage(TRUE);
            break;
        }
        return TRUE;
    default:
        break;
    }
    if (szUser1[0] != '\0'
     && szUser2[0] != '\0'
     && sess1 != (struct sess_con *) NULL
     && sess2 != (struct sess_con *) NULL
     && szFileName[0] != '\0')
        EnableWindow(hIDOK, TRUE); /* Enable OK when all there */
    return FALSE;
}
/*
 * Sign-on to ORACLE
 */
static struct sess_con * do_oracle_session(HWND hwnd, LPCSTR sz)
{
    strcpy(&szCaption[0], sz);
    if (DialogBox(hInst, MAKEINTRESOURCE(DLG_ORASESSION), 
                              hwnd, DlgOracleSession ))
        return tmp_con;
    else
        return (struct sess_con *) NULL;
}
/******************************************************************************
 * Entry point - Main Program Start Here
 * VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
#ifdef LCC
    (void) freopen("stderr.log","wb", stderr);
    setbuf(stderr,NULL);
#endif
    if (parse_cmd_line((char *) lpCmdLine))
    {
#ifdef LCC
        (void) fclose(stderr);
#endif
        return 0;
    }
    hAccelTable = LoadAccelerators(hInst,MAKEINTRESOURCE(IDACCEL));
    hInst = hInstance;
    InitCommonControls(); 
#ifdef DEBUG
#ifdef LCC
       stdout = fopen("stdout.log","wb");
       setbuf(stdout,NULL);
#endif
#endif
    if (!InitApplication())
        return 0;
    if ((hwndMain = CreateDialogParam(hInstance,
                MAKEINTRESOURCE(DLG_E2_SCHEMA_CMP), NULL, DlgE2SchemaCmp, 0L ))
            == NULL)
    {
        ShowError((LPSTR) "e2_schem_cmp",
         (LPSTR) "CreateDialogParam Failed, Error %u", GetLastError());
        return 0;
    }
    else
    {
        ShowWindow(hwndMain, SW_SHOW);
        ShowWindow(hwndMain, SW_SHOW);
        return do_message_loop();
    }
}
int parse_cmd_line(char * szLine)
{
char * fifo_args[30];                           /* Dummy arguments to process */
short int i;
/*
 * Process the arguments in the string that has been read
 */
    if ((fifo_args[1]=strtok(szLine,"  \n"))==NULL)
         return 0;
/*
 * Generate an argument vector
 */
    for (i=2;
             i < 30 && (fifo_args[i]=strtok(NULL," \n")) != (char *) NULL;
                 i++);
 
    fifo_args[0] = "";
    opterr=1;                /* turn off getopt() messages */
    optind=1;                /* reset to start of list */
    return proc_args(i,fifo_args);
}
#else
int main(argc, argv)
int argc;
char ** argv;
{
int i = proc_args(argc, argv);
    exit(!i);
}
void ShowError( x, x1,  x2, u)
char * x;
char *x1;
char * x2;
long int u;
{
    fputs(x, stderr);
    fputc('\n', stderr);
    fprintf(stderr, x1, x2, u);
    fputc('\n', stderr);
    return;
}
void scarper(file_name,line,message)
char * file_name;
int line;
char * message;
{
    (void) fprintf(stderr,"Unexpected Error %s,line %d\n",
                   file_name,line);
    perror(message);
    (void) fprintf(stderr,"UNIX Error Code %d\n", errno);
    (void) fputs("Program Terminating\n", stderr);
    exit(1);
}
#endif
int proc_args(argc,  argv)
int argc;
char ** argv;
{
int ch;
char * user1 = (char *) NULL; 
char * user2 = (char *) NULL;
char * filename = "-";
struct sess_con * sess1 = (struct sess_con *) NULL;
struct sess_con * sess2 = (struct sess_con *) NULL;
char * sav_sign;

static char * hlp = "e2schem_cmp Parameters:\n\
-1 First database connect string\n\
-d First database schema\n\
-2 Second database connect string\n\
-i Second database schema\n\
-f Output filename (default stdout)\n";

    while ((ch = getopt(argc, argv, "1:d:2:i:f:h")) != EOF)
    {
        switch (ch)
        {
        case '1':
            if ((sess1 = dyn_connect(strdup(optarg), "e2schem_cmp"))
                         == (struct sess_con *) NULL)
            {
                (void) ShowError("proc_args()", "First log on %s failed\n",
                       optarg, 0L);
                return 0;
            }
            else
                strcpy(sess1->description,optarg);
            sav_sign = optarg;
        case '2':
            if ((sess2 = dyn_connect(strdup(optarg), "e2schem_cmp"))
                         == (struct sess_con *) NULL)
            {
                (void) ShowError("proc_args()", "Second log on %s failed\n",
                       optarg, 0L);
                return 0;
            }
            else
                strcpy(sess2->description,optarg);
            break;
        case 'd':
            if (strlen(optarg))
                user1=optarg;
            break;
        case 'i':
            if (strlen(optarg))
                user2=optarg;
            break;
        case 'f':
            if (strlen(optarg))
                filename = optarg;
            break;
        case 'h':
        default:
            fputs( hlp, stderr);
            return 0;
        }
    }
    if (user1 != (char *) NULL && user2 != (char *) NULL
     && sess1 != (struct sess_con *) NULL
     && sess2 == (struct sess_con *) NULL)
        sess2 = dyn_connect(strdup(sav_sign), "e2schem_cmp");
    if ((user1 != (char *) NULL || user2 != (char *) NULL)
     && (sess1 != (struct sess_con *) NULL
     || sess2 != (struct sess_con *) NULL))
    {
        (void) schm_cmp_main(sess1, sess2, user1, user2, filename);
        return 1;
    }
    else
        return 0;
}
