/***************************************************************************
 * fish.c - Pull out arbitrary length files from within an archive of a type
 * that defies normal UNIX utility access
 */
#include <stdio.h>
#include <stdlib.h>
static char *sccs_id = "@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1994";
static int offset[256];
static int len[256];
static char * to_ext[256];
static int ext_list(argc,argv)
int argc;
char ** argv;
{
    int i = 2;
    int j = 0;
    while (i < argc)
    {
        to_ext[j] = argv[i++];
        offset[j] = atoi(argv[i++]);
        len[j] = atoi(argv[i++]);
        j++;
    }
    return j;
}
/*
 * Strip the files out of an archive from some foreign machine, where
 * the file positions and lengths can be identified by hand.
 * Parameters:
 * - name of archive file
 * - output file name
 * - offset in the archive file
 * - file length
 */
main (argc,argv)
int argc;
char ** argv;
{
int so_far;
char  buf[512];
int to_do;
char out_buf[BUFSIZ];
FILE * fpr;
FILE * fpw;
    if (argc < 2)
    {
        fprintf(stderr,
     "Provide an input file, a filename to strip, offset and len\n");
        exit(1);
    }
    else
        to_do = ext_list(argc,argv);
    if ((fpr = fopen(argv[1],"r")) == (FILE *) NULL)
    {
        perror("Input open");
        fprintf(stderr,"Cannot open filename %s\n", argv[1]);
        exit(1);
    }
#ifdef DEBUG
    for (so_far=0; so_far < to_do; so_far++)
    {
        printf("so_far=%d to_ext=%s offset=%d len=%d\n",so_far,
        to_ext[so_far],
        offset[so_far],
        len[so_far]);
    }
#endif
    for (so_far = 0; so_far < to_do; so_far++)
    {
    int residue;
    int blocks;
        fseek(fpr,offset[so_far],0);
        if ((fpw = fopen(to_ext[so_far],"w")) == (FILE *) NULL)
        {
            perror("Output open");
            fprintf(stderr,"Cannot open filename %s\n", to_ext[so_far]);
            exit(1);
        }
        setbuf(fpw,out_buf);
        blocks = len[so_far]/512;
        residue = len[so_far] % 512;
        while (blocks--)
        {
            fread(buf,sizeof(char),512,fpr);
            fwrite(buf,sizeof(char),512,fpw);
        }
        if (residue)
        {
            fread(buf,sizeof(char),512,fpr);
            fwrite(buf,sizeof(char),residue,fpw);
        }
        fclose(fpw);
    }
    exit(0);
}
