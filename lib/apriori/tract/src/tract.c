/*----------------------------------------------------------------------
  File    : tract.c
  Contents: item and transaction management
  Author  : Christian Borgelt
  History : 1996.02.14 file created as apriori.c
            1996.06.24 item reading optimized (function readitem())
            1996.07.01 adapted to modified module symtab
            1998.01.04 scan functions moved to module tabread
            1998.06.09 array enlargement modified
            1998.06.20 adapted to changed st_create function
            1998.08.07 bug in function ib_read() fixed
            1998.08.08 item appearances added
            1998.08.17 item sorting and recoding added
            1999.10.22 bug in item appearances reading fixed
            1999.11.11 adapted to name/identifier maps
            1999.12.01 check of item appearance added to sort function
            2000.03.15 removal of infrequent items added
            2001.07.14 adapted to modified module tabread
            2001.11.18 transaction functions added to this module
            2001.12.28 first version of this module completed
            2002.01.12 empty field at end of record reported as error
            2002.02.06 item sorting made flexible (different orders)
            2002.02.19 transaction tree functions added
            2003.07.17 function tbg_filter() added (remove unused items)
            2003.08.15 bug in function tat_delete() fixed
            2003.08.21 parameter 'heap' added to function tbg_sort()
            2003.09.20 empty transactions in input made possible
            2003.12.18 padding for align8 architecture added
            2004.02.26 item frequency counting moved to ib_read()
            2005.06.20 function nocmp() for neutral sorting added
            2007.02.13 adapted to modified module tabread
            2008.08.11 item appearance indicator decoding improved
            2008.08.12 considerable redesign, some new functions
            2008.08.14 sentinels added to item arrays in transactions
            2008.11.13 item and transaction sorting generalized
            2008.11.19 transaction tree and tree node separated
            2009.05.28 bug in function tbg_filter() fixed (minimal size)
            2009.08.27 fixed definitions of trans. tree node functions
            2010.03.16 handling of extended transactions added
            2010.07.02 transaction size comparison functions added
            2010.07.13 function tbg_reduce() optimized (size comparison)
            2010.08.10 function tbg_trim() added (for sequences)
            2010.08.11 parameter of ib_read() changed to general mode
            2010.08.13 function tbg_addib() added (add from item base)
            2010.08.19 function ib_readsel() added (item selectors)
            2010.08.22 adapted to modified module tabread
            2010.08.30 handling of empty transactions corrected
            2010.09.13 function tbg_mirror() added (mirror transactions)
            2010.10.08 adapted to modfied module tabread
            2010.12.07 added several explicit type casts (for C++)
            2010.12.15 functions tbg_read() and tbg_recount() added
            2010.12.20 functions tbg_icnts() and tbg_ifrqs() added
            2011.03.20 item reading moved from readitem() to ib_read()
            2011.05.05 tbg_count() adapted to extended transactions
            2011.05.09 bug in reading weighted items fixed (duplicates)
            2011.07.09 interface for transaction bag recoding modified
            2011.07.12 adapted to modified symbol table/idmap interface
            2011.07.13 read functions adapted to optional integer names
            2011.07.18 alternative transaction tree implementation added
            2011.08.11 bug in function ib_read() fixed (APP_NONE)
            2011.08.23 negative transaction weights no longer prohibited
            2011.08.25 prefix and suffix packing functions added
            2011.09.02 bin sort for transactions added (faster)
            2011.09.17 handling of filling end markers in transactions
            2011.09.28 bin sort for trans. extended to packed items
            2012.03.22 function ta_cmplim() added (limited comparison)
            2012.05.25 function taa_reduce() added (for occ. deliver)
            2012.05.30 version of taa_reduce() with hash table added
            2012.06.15 number of packed items stored in transaction bag
            2012.07.03 twin prime table for hash table sizes added
            2012.07.19 optional parentheses around transaction weight
            2012.07.21 function tbg_ipwgt() added (idempotent weights)
            2012.07.23 functions ib_write() and tbg_write() added
            2012.07.26 function tbg_ipwgt() improved with item arrays
            2012.07.27 duplicate transactions in tbg_ipgwt() eliminated
            2012.07.30 item weight update added to function tbg_ipwgt()
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include "tract.h"
#include "scanner.h"
#ifndef NOMAIN
#include "error.h"
#endif
#ifdef STORAGE
#include "storage.h"
#endif

#if defined _MSC_VER && !defined snprintf
#define snprintf _snprintf
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "tract"
#define DESCRIPTION "benchmarking transaction sorting methods"
#define VERSION     "version 1.7 (2012.07.30)         " \
                    "(c) 2011-2012   Christian Borgelt"

/* --- error codes --- */
/* error codes   0 to  -4 defined in tract.h */
#define E_STDIN      (-5)       /* double assignment of stdin */
#define E_OPTION     (-6)       /* unknown option */
#define E_OPTARG     (-7)       /* missing option argument */
#define E_ARGCNT     (-8)       /* too few/many arguments */
#define E_ITEMCNT    (-9)       /* invalid number of items */
/* error codes -15 to -25 defined in tract.h */

#define BLKSIZE      1024       /* block size for enlarging arrays */
#define TH_INSERT       8       /* threshold for insertion sort */
#define TS_PRIMES    (sizeof(primes)/sizeof(*primes))

#ifndef QUIET                   /* if not quiet version, */
#define MSG         fprintf     /* print messages */
#else                           /* if quiet version, */
#define MSG(...)                /* suppress messages */
#endif

#define SEC_SINCE(t)  ((clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef int SUBFN  (const TRACT  *t1, const TRACT  *t2, int off);
typedef int SUBWFN (const WTRACT *t1, const WTRACT *t2, int off);

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static WITEM  WTA_END  = {-1,0};/* sentinel for weighted items */
static WITEM  WTA_TERM = { 0,0};/* termination item */
static int    sorted   = 0;     /* flag for sorted indicators */
static CCHAR* appmap[] = {      /* item appearance indicators */
  "0:-",  "0:none",  "0:neither", "0:ignore",
  "1:i",  "1:in",    "1:a",  "1:antecedent", "1:b", "1:body",
  "2:o",  "2:out",   "2:c",  "2:consequent", "2:h", "2:head",
  "3:io", "3:inout", "3:ac", "3:a&c", "3:both", "3:bh", "3:b&h",
};                              /* (code:name) */

static int primes[] = {         /* table of twin prime numbers */
  19, 31, 61, 109, 241, 463, 1021, 2029, 4093, 8089, 16363, 32719,
  65521, 131011, 262111, 524221, 1048573, 2097133, 4193803, 8388451,
  16777141, 33554011, 67108669, 134217439, 268435009, 536870839,
  1073741719, 2147482951 };     /* (table contains larger twins) */

/* 4294965841L, 8589934291L, 17179868809L, 34359737299L, 68719476391L,
   137438953273L, 274877906629L, 549755813359L, 1099511626399L,
   2199023255479L, 4398046509739L, 8796093021409L, 17592186044299L,
   35184372088321L, 70368744176779L, 140737488353701L, 281474976710131L,
   562949953419319L, 1125899906841973L, 2251799813684461L,
   4503599627369863L, 9007199254738543L, 18014398509481729L,
   36028797018963799L, 72057594037925809L, 144115188075854269L,
   288230376151709779L, 576460752303422431L, 1152921504606845473L,
   2305843009213692031L, 4611686018427386203L, 9223372036854774511L */

static CCHAR *emsgs[] = {       /* error messages */
  /* E_NONE      0 */  "no error",
  /* E_NOMEM    -1 */  "not enough memory",
  /* E_FOPEN    -2 */  "cannot open file %s",
  /* E_FREAD    -3 */  "read error on file %s",
  /* E_FWRITE   -4 */  "write error on file %s",
  /*     -5 to  -9 */  NULL, NULL, NULL, NULL, NULL,
  /*    -10 to -14 */  NULL, NULL, NULL, NULL, NULL,
  /* E_NOITEMS -15 */  "no (frequent) items found",
  /* E_ITEMEXP -16 */  "#item expected",
  /* E_ITEMWGT -17 */  "#invalid item weight %s",
  /* E_DUPITEM -18 */  "#duplicate item '%s'",
  /* E_INVITEM -19 */  "#invalid item '%s' (no integer)",
  /* E_WGTEXP  -20 */  "#transaction weight expected",
  /* E_TAWGT   -21 */  "#invalid transaction weight %s",
  /* E_FLDCNT  -22 */  "#too many fields/columns",
  /* E_APPEXP  -23 */  "#appearance indicator expected",
  /* E_UNKAPP  -24 */  "#unknown appearance indicator '%s'",
  /* E_PENEXP  -25 */  "#insertion penalty expected",
  /* E_PENALTY -26 */  "#invalid insertion penalty %s",
  /*           -27 */  "unknown error"
};

#if !defined QUIET && !defined NOMAIN
/* --- error messages --- */
static const char *errmsgs[] = {
  /* E_NONE      0 */  "no error",
  /* E_NOMEM    -1 */  "not enough memory",
  /* E_FOPEN    -2 */  "cannot open file %s",
  /* E_FREAD    -3 */  "read error on file %s",
  /* E_FWRITE   -4 */  "write error on file %s",
  /* E_STDIN    -5 */  "double assignment of standard input",
  /* E_OPTION   -6 */  "unknown option -%c",
  /* E_OPTARG   -7 */  "missing option argument",
  /* E_ARGCNT   -8 */  "wrong number of arguments",
  /* E_ITEMCNT  -9 */  "invalid number of items (must be <= 16)",
  /*    -10 to -14 */  NULL, NULL, NULL, NULL, NULL,
  /* E_NOITEMS -15 */  "no (frequent) items found",
  /*           -16 */  "unknown error"
};
#endif

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
#ifndef NOMAIN
#ifndef QUIET
static CCHAR    *prgname;       /* program name for error messages */
#endif
static TABREAD  *tread = NULL;  /* table/transaction reader */
static ITEMBASE *ibase = NULL;  /* item base */
static TABAG    *tabag = NULL;  /* transaction bag/multiset */
#endif

static char msgbuf[2*TRD_MAXLEN+64];  /* buffer for error messages */

/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/

static int appcmp (const void *p1, const void *p2, void *data)
{                               /* --- compare appearance indicators */
  CCHAR *s1 = (CCHAR*)p1 +2;    /* type the two pointers */
  CCHAR *s2 = (CCHAR*)p2 +2;    /* to strings (skip code) */
  int   d;                      /* difference of character values */

  for ( ; 1; s1++, s2++) {      /* traverse the strings */
    d = (unsigned char)*s1 -(unsigned char)*s2;
    if (d   != 0) return d;     /* if strings differ, return result */
    if (*s1 == 0) return 0;     /* if at the end of the string, */
  }                             /* return 'strings are equal' */
}  /* appcmp() */

/*--------------------------------------------------------------------*/

static int appcode (CCHAR *s)
{                               /* --- get appearance indicator code */
  int   i, n;                   /* index, number of app. indicators */
  CCHAR *t;                     /* to traverse the indicator name */

  assert(s);                    /* check the function argument */
  n = (int)(sizeof(appmap)/sizeof(*appmap));
  if (!sorted) {                /* if app. indicators not sorted yet */
    ptr_qsort((void*)appmap, n, appcmp, NULL);
    sorted = -1;                /* sort the appearance indicators */
  }                             /* and set the sorted flag */
  i = ptr_bsearch(s-2, (void*)appmap, n, appcmp, NULL);
  if (i >= n) return -1;        /* try to find appearance indicator */
  if (i >= 0)                   /* if an exact match was found, */
    return appmap[i][0] -'0';   /* return the app. indicator code */
  t = appmap[i = -1-i] +2;      /* otherwise check for a prefix match */
  while (*s && (*s == *t)) s++, t++;
  if (*s) return -1;            /* if no match, abort the function */
  return appmap[i][0] -'0';     /* return the app. indicator code */
}  /* appcode() */

/*--------------------------------------------------------------------*/

static int nocmp (const void *p1, const void *p2, void *data)
{                               /* --- compare item identifiers */
  const ITEM *a = (const ITEM*)p1;    /* type the item pointers */
  const ITEM *b = (const ITEM*)p2;

  if (a->app == APP_NONE) return (b->app == APP_NONE) ? 0 : 1;
  if (b->app == APP_NONE) return -1;
  return a->id -b->id;          /* return sign of identifier diff. */
}  /* nocmp() */

/* Note that forming a simple difference cannot lead to an overflow,  */
/* because the two identifiers are guaranteed to be non-negative and  */
/* therefore the sign of the formed difference is certainly correct.  */
/* The same holds for the frequencies in the following functions.     */
/*--------------------------------------------------------------------*/

static int asccmp (const void *p1, const void *p2, void *data)
{                               /* --- compare item frequencies */
  const ITEM *a = (const ITEM*)p1;    /* type the item pointers */
  const ITEM *b = (const ITEM*)p2;

  if (a->app == APP_NONE) return (b->app == APP_NONE) ? 0 : 1;
  if (b->app == APP_NONE) return -1;
  return a->frq -b->frq;        /* return sign of frequency diff. */
}  /* asccmp() */

/*--------------------------------------------------------------------*/

static int descmp (const void *p1, const void *p2, void *data)
{                               /* --- compare item frequencies */
  const ITEM *a = (const ITEM*)p1;    /* type the item pointers */
  const ITEM *b = (const ITEM*)p2;

  if (a->app == APP_NONE) return (b->app == APP_NONE) ? 0 : 1;
  if (b->app == APP_NONE) return -1;
  return b->frq -a->frq;        /* return sign of frequency diff. */
}  /* descmp() */

/*--------------------------------------------------------------------*/

static int asccmpx (const void *p1, const void *p2, void *data)
{                               /* --- compare item frequencies */
  const ITEM *a = (const ITEM*)p1;    /* type the item pointers */
  const ITEM *b = (const ITEM*)p2;

  if (a->app == APP_NONE) return (b->app == APP_NONE) ? 0 : 1;
  if (b->app == APP_NONE) return -1;
  return a->xfq -b->xfq;        /* return sign of frequency diff. */
}  /* asccmpx() */

/*--------------------------------------------------------------------*/

static int descmpx (const void *p1, const void *p2, void *data)
{                               /* --- compare item frequencies */
  const ITEM *a = (const ITEM*)p1;    /* type the item pointers */
  const ITEM *b = (const ITEM*)p2;

  if (a->app == APP_NONE) return (b->app == APP_NONE) ? 0 : 1;
  if (b->app == APP_NONE) return -1;
  return b->xfq -a->xfq;        /* return sign of frequency diff. */
}  /* descmpx() */

/*----------------------------------------------------------------------
  Item Base Functions
----------------------------------------------------------------------*/

ITEMBASE* ib_create (int mode, int size)
{                               /* --- create an item base */
  ITEMBASE *base;               /* created item base */
  TRACT    *t;                  /* a transaction */
  WTRACT   *x;                  /* a transaction with weighted items */

  if (size <= 0) size = BLKSIZE;/* check and adapt number of items */
  base = (ITEMBASE*)malloc(sizeof(ITEMBASE));
  if (!base) return NULL;       /* create item base and components */
  base->idmap = (mode & IB_INTNAMES)
              ? idm_create(0, 0, ST_INTFN, (OBJFN*)0)
              : idm_create(0, 0, ST_STRFN, (OBJFN*)0);
  if (!base->idmap) { free(base); return NULL; }
  base->mode = mode;            /* initialize the fields */
  base->wgt  = 0;               /* there are no transactions yet */
  base->app  = APP_BOTH;        /* default: appearance in body & head */
  base->pen  = 0.0;             /* default: no item insertion allowed */
  base->idx  = 1;               /* index of current transaction */
  base->size = size;            /* size of transaction buffer */
  if (mode & IB_WEIGHTS) {      /* if items carry weights */
    base->tract = x = (WTRACT*)malloc(sizeof(WTRACT)
                           +(size+1) *sizeof(WITEM));
    if (x) { x->size = x->wgt = 0;
             x->items[0] = x->items[size+1] = WTA_END; }}
  else {                        /* if items do not carry weights */
    base->tract = t = (TRACT*) malloc(sizeof(TRACT)
                           +(size+1) *sizeof(int));
    if (t) { t->size = t->wgt = 0;
             t->items[0] = t->items[size+1] = TA_END; }
  }                             /* clear the transaction buffer */
  if (!base->tract) { ib_delete(base); return NULL; }
  base->err = 0;                /* clear the error code and */
  base->trd = NULL;             /* the table/transaction reader */
  return base;                  /* return the created item set */
}  /* ib_create() */

/*--------------------------------------------------------------------*/

void ib_delete (ITEMBASE *base)
{                               /* --- delete an item set */
  assert(base);                 /* check the function argument */
  if (base->tract) free(base->tract);
  if (base->idmap) idm_delete(base->idmap);
  free(base);                   /* delete the components */
}  /* ib_delete() */            /* and the item base body */

/*--------------------------------------------------------------------*/

int ib_add (ITEMBASE *base, const void *name)
{                               /* --- add an item to the set */
  int  size;                    /* size of the item name */
  ITEM *item;                   /* to access the item data */

  assert(base && name);         /* check the function arguments */
  size = (base->mode & IB_INTNAMES)
       ? sizeof(int) : strlen((const char*)name)+1;
  item = (ITEM*)idm_add(base->idmap, name, size, sizeof(ITEM));
  if (item == NULL)   return -1;/* add the new item */
  if (item == EXISTS) return -2;/* to the name/id map */
  item->app = base->app;        /* init. the appearance indicator */
  item->idx = item->xfq = item->frq = 0;
  item->pen = base->pen;        /* clear counters and trans. index, */
  return item->id;              /* init. the insertion penalty and */
}  /* ib_add() */               /* return the item identifier */

/*--------------------------------------------------------------------*/

const char* ib_xname (ITEMBASE *base, int item)
{                               /* --- get an item name */
  static char buf[32];          /* buffer for integer formatting */

  assert(base && (item >= 0));  /* check the function arguments */
  if (!(base->mode & IB_INTNAMES))
    return ib_name(base, item); /* if possible, return name directly */
  snprintf(buf, sizeof(buf), "%d", ib_int(base, item));
  return buf;                   /* otherwise format the integer */
}  /* ib_xname() */

/*--------------------------------------------------------------------*/

int ib_add2ta (ITEMBASE *base, const void *name)
{                               /* --- add an item to transaction */
  int   size;                   /* size of item name/buffer */
  ITEM  *item;                  /* to access the item data */
  TRACT *t;                     /* to access the transaction */

  assert(base && name);         /* check the function arguments */
  item = (ITEM*)idm_bykey(base->idmap, name);
  if (!item) {                  /* get the item by its key/name */
    size = (base->mode & IB_INTNAMES)
         ? sizeof(int) : strlen((const char*)name)+1;
    item = (ITEM*)idm_add(base->idmap, name, size, sizeof(ITEM));
    if (item == NULL) return -1;/* add a new item to the base */
    item->app = base->app;      /* init. the appearance indicator */
    item->idx = item->xfq = item->frq = 0;
    item->pen = base->pen;      /* clear counters and trans. index */
  }                             /* and init. the insertion penalty */
  t = (TRACT*)base->tract;      /* get the transaction buffer */
  if (item->idx >= base->idx)   /* if the item is already contained, */
    return t->size;             /* simply abort the function */
  item->idx = base->idx;        /* update the transaction index */
  size = base->size;            /* get the current buffer size */
  if (t->size >= size) {        /* if the transaction buffer is full */
    size += (size > BLKSIZE) ? (size >> 1) : BLKSIZE;
    t  = (TRACT*)realloc(t, sizeof(TRACT) +size *sizeof(int));
    if (!t) return -1;          /* enlarge the transaction buffer */
    t->items[base->size = size] = TA_END; base->tract = t;
  }                             /* store the new buffer and its size */
  t->items[t->size] = item->id; /* store the item and */
  return ++t->size;             /* return the new transaction size */
}  /* ib_add2ta() */

/*--------------------------------------------------------------------*/

void ib_finta (ITEMBASE *base, int wgt)
{                               /* --- finalize transaction buffer */
  int   i;                      /* loop variable */
  TRACT *t;                     /* to access the transaction buffer */
  ITEM  *item;                  /* to access the item data */

  assert(base);                 /* check the function arguments */
  t = (TRACT*)base->tract;      /* get the transaction buffer and */
  t->items[t->size] = TA_END;   /* store a sentinel at the end */
  base->wgt += t->wgt = wgt;    /* sum the transaction weight and */
  wgt *= t->size;               /* compute extended frequency weight */
  for (i = 0; i < t->size; i++) {
    item = (ITEM*)idm_byid(base->idmap, t->items[i]);
    item->frq += t->wgt;        /* traverse the items and */
    item->xfq += wgt;           /* sum the transaction weights */
  }                             /* and the transaction sizes */
  ++base->idx;                  /* update the transaction index */
}  /* ib_finta() */

/*--------------------------------------------------------------------*/

int ib_readsel (ITEMBASE *base, TABREAD *trd)
{                               /* --- read item selection */
  int  d, i;                    /* delimiter type, item */
  char *b, *e;                  /* buffer for a field, end pointer */
  ITEM *item;                   /* to access the item data */

  assert(base && trd);          /* check the function arguments */
  base->trd = trd;              /* note the table reader and set */
  base->app = APP_NONE;         /* the default appearance indicator */
  while (1) {                   /* read a simple list of items */
    d = trd_read(trd);          /* read the next item */
    if (d <= TRD_ERR)     return base->err = E_FREAD;
    if (d <= TRD_EOF)     return base->err = idm_cnt(base->idmap);
    b = trd_field(trd);         /* check whether an item was read */
    if (!*b)              return base->err = E_ITEMEXP;
    if (!(base->mode & IB_INTNAMES))
      item = (ITEM*)idm_add(base->idmap, b,trd_len(trd)+1,sizeof(ITEM));
    else {                      /* if string names, add directly */
      i = (int)strtol(b, &e,0); /* otherwise convert to integer */
      if (*e || (e == b)) return base->err = E_INVITEM;
      item = (ITEM*)idm_add(base->idmap, &i, sizeof(int), sizeof(ITEM));
    }                           /* check integer and add item */
    if (item == NULL)     return base->err = E_NOMEM;
    if (item == EXISTS)   continue;   /* add new item, skip known */
    item->app = APP_BOTH;       /* init. the appearance indicator */
    item->idx = item->xfq = item->frq = 0;
    item->pen = base->pen;      /* clear counters and trans. index */
  }                             /* and init. the insertion penalty */
}  /* ib_readsel() */

/*--------------------------------------------------------------------*/

int ib_readapp (ITEMBASE *base, TABREAD *trd)
{                               /* --- read appearance indicators */
  int  d, i;                    /* delimiter type, app. indicator */
  char *b, *e;                  /* buffer for a field, end pointer */
  ITEM *item;                   /* to access the item data */

  assert(base && trd);          /* check the function arguments */
  base->trd = trd;              /* note the table reader */
  d = trd_read(trd);            /* read the first record */
  if (d <= TRD_ERR)       return base->err = E_FREAD;
  if (d != TRD_REC)       return base->err = E_FLDCNT;
  i = appcode(trd_field(trd));  /* get and check appearance code */
  if (i < 0)              return base->err = E_UNKAPP;
  base->app = i;                /* store default appearance indicator */
  while (1) {                   /* read item/indicator pairs */
    d = trd_read(trd);          /* read the next item */
    if (d <= TRD_ERR)     return base->err = E_FREAD;
    if (d <= TRD_EOF)     return base->err = idm_cnt(base->idmap);
    b = trd_field(trd);         /* check whether an item was read */
    if (!*b)              return base->err = E_ITEMEXP;
    if (!(base->mode & IB_INTNAMES))
      item = (ITEM*)idm_add(base->idmap, b,trd_len(trd)+1,sizeof(ITEM));
    else {                      /* if string names, add directly */
      i = (int)strtol(b, &e,0); /* otherwise convert to integer */
      if (*e || (e == b)) return base->err = E_INVITEM;
      item = (ITEM*)idm_add(base->idmap, &i, sizeof(int), sizeof(ITEM));
    }                           /* check integer and add item */
    if (item == NULL)     return base->err = E_NOMEM;
    if (item == EXISTS)   return base->err = E_DUPITEM;
    item->app = base->app;      /* init. the appearance indicator */
    item->idx = item->xfq = item->frq = 0;
    item->pen = base->pen;      /* clear counters and trans. index */
    if (d != TRD_FLD)     return base->err = E_APPEXP;
    d = trd_read(trd);          /* read the item appearance code */
    if (d <= TRD_ERR)     return base->err = E_FREAD;
    if (d == TRD_FLD)     return base->err = E_FLDCNT;
    i = appcode(trd_field(trd));
    if (i < 0)            return base->err = E_UNKAPP;
    item->app = i;              /* get, check and store */
  }                             /* the appearance indicator */
}  /* ib_readapp() */

/*--------------------------------------------------------------------*/

int ib_readpen (ITEMBASE *base, TABREAD *trd)
{                               /* --- read insertion penalties */
  int    d, i;                  /* delimiter type, item */
  double p;                     /* item insertion penalty */
  char   *b, *e;                /* buffer for a field, end pointer */
  ITEM   *item;                 /* to access the item data */

  assert(base && trd);          /* check the function arguments */
  base->trd = trd;              /* note the table reader */
  d = trd_read(trd);            /* read the first record */
  if (d <= TRD_ERR)       return base->err = E_FREAD;
  if (d != TRD_REC)       return base->err = E_FLDCNT;
  b = trd_field(trd);           /* get default insertion penalty */
  p = strtod(b, &e);            /* and convert and check it */
  if (*e || (e == b) || (p > 1)) return base->err = E_PENALTY;
  if (p < 0) { base->app = APP_NONE; p = 0; }
  else       { base->app = APP_BOTH; }
  base->pen = p;                /* store default insertion penalty */
  while (1) {                   /* read item/indicator pairs */
    d = trd_read(trd);          /* read the next item */
    if (d <= TRD_ERR)     return base->err = E_FREAD;
    if (d <= TRD_EOF)     return base->err = idm_cnt(base->idmap);
    b = trd_field(trd);         /* check whether an item was read */
    if (!*b)              return base->err = E_ITEMEXP;
    if (!(base->mode & IB_INTNAMES))
      item = (ITEM*)idm_add(base->idmap, b,trd_len(trd)+1,sizeof(ITEM));
    else {                      /* if string names, add directly */
      i = (int)strtol(b, &e,0); /* otherwise convert to integer */
      if (*e || (e == b)) return base->err = E_INVITEM;
      item = (ITEM*)idm_add(base->idmap, &i, sizeof(int), sizeof(ITEM));
    }                           /* check integer and add item */
    if (item == NULL)     return base->err = E_NOMEM;
    if (item == EXISTS)   return base->err = E_DUPITEM;
    item->app = base->app;      /* init. the appearance indicator */
    item->idx = item->xfq = item->frq = 0;
    item->pen = base->pen;      /* clear counters and trans. index */
    if (d != TRD_FLD)     return base->err = E_PENEXP;
    d = trd_read(trd);          /* read the insertion penalty */
    if (d <= TRD_ERR)     return base->err = E_FREAD;
    if (d == TRD_FLD)     return base->err = E_FLDCNT;
    b = trd_field(trd);         /* get the insertion penalty */
    p = strtod(b, &e);          /* and convert and check it */
    if (*e || (e == b) || (p > 1)) return base->err = E_PENALTY;
    if (p < 0) { item->app = APP_NONE; p = 0; }
    else       { item->app = APP_BOTH; }
    item->pen = p;              /* store item appearence indicator */
  }                             /* and the insertion penalty */
}  /* ib_readpen() */

/*--------------------------------------------------------------------*/

int ib_read (ITEMBASE *base, TABREAD *trd, int mode)
{                               /* --- read a transaction */
  int    i, w;                  /* loop variable/buffer, item weight */
  int    d, n;                  /* delimiter type, array size */
  char   *b, *e;                /* read buffer and end pointer */
  ITEM   *item;                 /* to access the item data */
  WITEM  *wit;                  /* to access the item data (weighted) */
  TRACT  *t;                    /* to access the transaction */
  WTRACT *x;                    /* to access the transaction */
  double z;                     /* buffer for an item weight */

  assert(base && trd);          /* check the function arguments */
  base->trd = trd;              /* note the table reader */
  ++base->idx;                  /* update the transaction index and */
  x = (WTRACT*)base->tract;     /* get the transaction buffers */
  t = (TRACT*) base->tract;     /* set the default transaction weight */
  t->wgt = 1; t->size = 0;      /* and init. the transaction size */
  do {                          /* read the transaction items */
    d = trd_read(trd);          /* read the next field (item name) */
    if (d <= TRD_ERR) return base->err = E_FREAD;
    if (d <= TRD_EOF) return base->err = 1;
    b = trd_field(trd);         /* check whether field is empty */
    if ((d == TRD_REC)          /* if at the last field of a record */
    &&  (mode & TA_WEIGHT)) {   /* and to read transaction weights */
      if (!*b) return base->err = E_WGTEXP;
      w = ((*b == '(') || (*b == '[') || (*b == '{'))
        ? *b++ : 0;             /* skip an opening parenthesis */
      t->wgt = strtol(b, &e,0); /* get the weight (integer number) */
      if ((e == b) || (w && !*e))
        return base->err = E_TAWGT;
      if (((w == '(') && (*e == ')'))
      ||  ((w == '[') && (*e == ']'))
      ||  ((w == '{') && (*e == '}')))
        e++;                    /* skip a closing parenthesis */
      if (*e) return base->err = E_TAWGT;
      break;                    /* check for following garbage and */
    }                           /* abort the item set read loop */
    if (!*b) {                  /* if the field is empty, */
      if (d == TRD_REC) break;  /* the transaction must be empty */
      return base->err = E_ITEMEXP;
    }                           /* otherwise there must be an item */
    if (!(base->mode & IB_INTNAMES)) {
      i = 0;                    /* if string names, get directly */
      item = (ITEM*)idm_byname(base->idmap, b); }
    else {                      /* if integer names, */
      i = (int)strtol(b, &e,0); /* convert name to integer */
      if (*e || (e == b)) return base->err = E_INVITEM;
      item = (ITEM*)idm_bykey(base->idmap, &i);
    }                           /* check integer and get item */
    if (!item) {                /* look up the name in name/id map */
      if (base->app == APP_NONE) { /* if new items are to be ignored */
        if ((base->mode & IB_WEIGHTS)
        &&  trd_istype(trd, trd_last(trd), TA_WGTSEP))
          d = trd_read(trd);    /* consume a possible item weight */
        if (d <= TRD_ERR) return base->err = E_FREAD;
        if (d == TRD_REC) break;/* if at the end of a record, */
        continue;               /* abort the item read loop, */
      }                         /* otherwise read the next field */
      if (!(base->mode & IB_INTNAMES))
        item = (ITEM*)idm_add(base->idmap,b,trd_len(trd)+1,sizeof(ITEM));
      else {                    /* if string names, add directly */
        item = (ITEM*)idm_add(base->idmap,&i,sizeof(int),sizeof(ITEM));
      }                         /* if integer names, use conversion */
      if (!item) return base->err = E_NOMEM;
      item->app = base->app;    /* add the new item to the map */
      item->idx = item->xfq = item->frq = 0;
      item->pen = base->pen;    /* clear counters and trans. index */
    }                           /* and init. the insertion penalty */
    if (item->idx >= base->idx){/* if the item is already contained, */
      if   (mode & TA_DUPERR)   /* check what to do with duplicates */
        return base->err = E_DUPITEM;
      if (!(mode & TA_DUPLICS)){/* if to ignore duplicates */
        if ((base->mode & IB_WEIGHTS)
        &&  trd_istype(trd, trd_last(trd), TA_WGTSEP)) {
          d = trd_read(trd);    /* read a given item weight */
          if (d <= TRD_ERR)   return base->err = E_FREAD;
          z = strtod(b = trd_field(trd), &e); (void)z;
          if ((e == b) || *e) return base->err = E_ITEMWGT;
        } continue;             /* check the read item weight */
      }                         /* (even though it will be ignored) */
    }                           /* and then read the next item */
    item->idx = base->idx;      /* update the transaction index */
    n = base->size;             /* get the current buffer size */
    if (base->mode & IB_WEIGHTS) { /* if the items carry weights */
      if (x->size >= n) {       /* if the transaction buffer is full */
        n += (n > BLKSIZE) ? (n >> 1) : BLKSIZE;
        x  = (WTRACT*)realloc(x, sizeof(WTRACT) +(n+1) *sizeof(WITEM));
        if (!x) return base->err = E_NOMEM;
        base->size = n; x->items[n+1] = WTA_END; base->tract = x;
      }                         /* enlarge the transaction buffer */
      wit = x->items +x->size++;/* get the next transaction item */
      wit->id = item->id;       /* and store the item identifier */
      if (!trd_istype(trd, trd_last(trd), TA_WGTSEP))
        wit->wgt = 1;           /* if no weight separator follows, */
      else {                    /* set default weight, otherwise */
        d = trd_read(trd);      /* read the given item weight */
        if (d <= TRD_ERR)   return base->err = E_FREAD;
        wit->wgt = (float)strtod(b = trd_field(trd), &e);
        if ((e == b) || *e) return base->err = E_ITEMWGT;
      } }                       /* decode and set the item weight */
    else {                      /* if the items do not carry weights */
      if (t->size >= n) {       /* if the transaction buffer is full */
        n += (n > BLKSIZE) ? (n >> 1) : BLKSIZE;
        t  = (TRACT*)realloc(t, sizeof(TRACT) +(n+1) *sizeof(int));
        if (!t) return base->err = E_NOMEM;
        base->size = n; t->items[n+1] = TA_END; base->tract = t;
      }                         /* enlarge the transaction buffer */
      t->items[t->size++] = item->id;
    }                           /* add the item to the transaction */
  } while (d == TRD_FLD);       /* while not at end of record */
  if (base->mode & IB_WEIGHTS){ /* if the items carry weights */
    x = (WTRACT*)base->tract;   /* get the transaction buffer and */
    if (mode & TA_TERM)         /* if requested, store term. item */
      x->items[x->size++] = WTA_TERM;
    x->items[x->size] = WTA_END;/* store sentinel after the items */
    base->wgt += x->wgt;        /* sum the transaction weight */
    w = x->size *x->wgt;        /* compute extended frequency weight */
    for (i = 0; i < x->size; i++) {
      item = (ITEM*)idm_byid(base->idmap, x->items[i].id);
      item->frq += x->wgt;      /* traverse the items and */
      item->xfq += w;           /* sum the transaction weights */
    } }                         /* and the transaction sizes */
  else {                        /* if the items do not carry weights */
    t = (TRACT*)base->tract;    /* get the transaction buffer and */
    if (mode & TA_TERM)         /* if requested, store term. item */
      t->items[t->size++] = 0;  /* (item 0 to indicate sequence end) */
    t->items[t->size] = TA_END; /* store a sentinel after the items */
    base->wgt += t->wgt;        /* sum the transaction weight and */
    w = t->size *t->wgt;        /* compute extended frequency weight */
    for (i = 0; i < t->size; i++) {
      item = (ITEM*)idm_byid(base->idmap, t->items[i]);
      item->frq += t->wgt;      /* traverse the items and */
      item->xfq += w;           /* sum the transaction weights */
    }                           /* and the transaction sizes */
  }                             /* as importance indicators */
  return base->err = 0;         /* return 'ok' */
}  /* ib_read() */

/*--------------------------------------------------------------------*/
#ifdef TA_WRITE

int ib_write (ITEMBASE *base, TABWRITE *twr, const char *wgtfmt, ...)
{                               /* --- write a transaction */
  va_list    args;              /* list of variable arguments */
  const char *iwfmt;            /* item weight format */

  assert(base && twr);          /* check the function arguments */
  if (!(base->mode & IB_WEIGHTS))
    return ta_write( (TRACT*)base->tract, base, twr, wgtfmt);
  va_start(args, wgtfmt);       /* start variable arguments */
  iwfmt = va_arg(args, const char*);
  wta_write((WTRACT*)base->tract, base, twr, wgtfmt, iwfmt);
  va_end(args);                 /* write the transactions and */
  return twr_error(twr);        /* return a write error indicator */
}  /* ib_write() */

#endif
/*--------------------------------------------------------------------*/

const char* ib_errmsg (ITEMBASE *base, char *buf, size_t size)
{                               /* --- get last (read) error message */
  int        i;                 /* index of error message */
  size_t     k = 0;             /* buffer for header length */
  const char *msg;              /* error message (format) */

  assert(base                   /* check the function arguments */
  &&    (!buf || (size > 0)));  /* if none given, get internal buffer */
  if (!buf) { buf = msgbuf; size = sizeof(msgbuf); }
  i = (base->err < 0) ? -base->err : 0;
  assert(i < (int)(sizeof(emsgs)/sizeof(char*)));
  msg = emsgs[i];               /* get the error message (format) */
  assert(msg);                  /* check for a proper message */
  if (*msg == '#') { msg++;     /* if message needs a header */
    k = snprintf(buf, size, "%s:%d(%d): ", TRD_FPOS(base->trd));
    if (k >= size) k = size-1;  /* print the input file name and */
  }                             /* the record and field number */
  snprintf(buf+k, size-k, msg, trd_field(base->trd));
  return buf;                   /* format the error message */
}  /* ib_errmsg() */

/*--------------------------------------------------------------------*/

int ib_recode (ITEMBASE *base, int min, int max,
               int cnt, int dir, int *map)
{                               /* --- recode items w.r.t. frequency */
  int    i, n;                  /* loop variables, item buffer */
  ITEM   *item;                 /* to traverse the items */
  TRACT  *t;                    /* to access the standard transaction */
  int    *s, *d;                /* to traverse the items */
  WTRACT *x;                    /* to access the extended transaction */
  WITEM  *a, *b;                /* to traverse the items */
  CMPFN  *cmp;                  /* comparison function */

  assert(base);                 /* check the function arguments */
  if (max < 0) max = INT_MAX;   /* adapt the maximum frequency */
  if (cnt < 0) cnt = INT_MAX;   /* adapt the maximum number of items */
  for (i = n = idm_cnt(base->idmap); --n >= 0; ) {
    item = (ITEM*)idm_byid(base->idmap, n);
    if ((item->frq < min) || (item->frq > max))
      item->app = APP_NONE;     /* set all items to 'ignore' */
  }                             /* that have an invalid frequency */
  if      (dir >  1) cmp = asccmpx;  /* get the appropriate */
  else if (dir >  0) cmp = asccmp;   /* comparison function */
  else if (dir >= 0) cmp = nocmp;    /* (ascending/descending) */
  else if (dir > -2) cmp = descmp;   /* and sort the items */
  else               cmp = descmpx;  /* w.r.t. their frequency */
  idm_sort(base->idmap, cmp, NULL, map, 1);
  for (i = n = idm_cnt(base->idmap); --n >= 0; )
    if (((ITEM*)idm_byid(base->idmap, n))->app != APP_NONE)
      break;                    /* find last non-ignorable item */
  if (++n > cnt) n = cnt;       /* limit the number of items */
  idm_trunc(base->idmap, n);    /* remove all items to ignore */
  if (!map) return n;           /* if no map is provided, abort */
  while (--i >= 0)              /* mark all removed items */
    if (map[i] >= n) map[i] = -1;
  if (base->mode & IB_WEIGHTS){ /* if the items carry weights */
    x = (WTRACT*)base->tract;   /* traverse the buffered transaction */
    for (a = b = x->items; a->id >= 0; a++) {
      i = map[a->id];           /* recode all items and */
      if (i >= 0) (b++)->id = i;/* remove all items to ignore */
    }                           /* from the buffered transaction */
    x->size = b -x->items;      /* compute the new number of items */
    x->items[x->size] = WTA_END; } /* store sentinel after the items */
  else {                        /* if the items do not carry weights */
    t = (TRACT*)base->tract;    /* traverse the buffered transaction */
    for (s = d = t->items; *s > TA_END; s++) {
      i = map[*s];              /* recode all items and */
      if (i >= 0) *d++ = i;     /* remove all items to ignore */
    }                           /* from the buffered transaction */
    t->size = d -t->items;      /* compute the new number of items */
    t->items[t->size] = TA_END; /* store a sentinel after the items */
  }
  return n;                     /* return number of frequent items */
}  /* ib_recode() */

/*--------------------------------------------------------------------*/

void ib_trunc (ITEMBASE *base, int cnt)
{                               /* --- truncate an item base */
  TRACT  *t;                    /* to access the standard transaction */
  int    *s, *d;                /* to traverse the items */
  WTRACT *x;                    /* to access the extended transaction */
  WITEM  *a, *b;                /* to traverse the items */

  assert(base && (cnt >= 0));   /* check the function arguments */
  idm_trunc(base->idmap, cnt);  /* truncate the item base */
  if (base->mode & IB_WEIGHTS){ /* if the items carry weights */
    x = (WTRACT*)base->tract;   /* traverse the buffered transaction */
    for (a = b = x->items; a->id >= 0; a++)
      if (a->id < cnt) *b++ = *a;      /* remove all deleted items */
    x->size = b -x->items;      /* compute the new number of items */
    x->items[x->size] = WTA_END; } /* store sentinel after the items */
  else {                        /* if the items do not carry weights */
    t = (TRACT*)base->tract;    /* traverse the buffered transaction */
    for (s = d = t->items; *s > TA_END; s++)
      if (*s < cnt) *d++ = *s;  /* remove all deleted items */
    t->size = d -t->items;      /* compute the new number of items */
    t->items[t->size] = TA_END; /* store a sentinel after the items */
  }                             /* (adapt the buffered transaction) */
}  /* ib_trunc() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void ib_show (ITEMBASE *base)
{                               /* --- show an item base */
  int  i;                       /* loop variable */
  ITEM *item;                   /* to traverse the items */

  assert(base);                 /* check the function argument */
  for (i = 0; i < idm_cnt(base->idmap); i++) {
    item = (ITEM*)idm_byid(base->idmap, i);
    printf("%-16s: ", idm_name(item));
    printf("id %6d, app ",       item->id);
    fputc((item->app & APP_HEAD) ? 'h' : '-', stdout);
    fputc((item->app & APP_BODY) ? 'b' : '-', stdout);
    printf("pen %8.6g, ",        item->pen);
    printf("frq %7d, xfq %7d\n", item->frq, item->xfq);
  }                             /* traverse items and print info */
  printf("%d item(s)\n", idm_cnt(base->idmap));
}  /* ib_show() */

#endif
/*----------------------------------------------------------------------
  Transaction Functions
----------------------------------------------------------------------*/

TRACT* ta_create (const int *items, int n, int wgt)
{                               /* --- create a transaction */
  TRACT *t;                     /* created transaction */

  assert(items || (n <= 0));    /* check the function arguments */
  t = (TRACT*)malloc(sizeof(TRACT) +((n > 0) ? n : 1) *sizeof(int));
  if (!t) return NULL;          /* allocate a new transaction */
  t->wgt = wgt;                 /* set weight and copy the items */
  memcpy(t->items, items, n *sizeof(int));
  t->items[t->size = n] = TA_END;
  return t;                     /* store a sentinel after the items */
}  /* ta_create() */            /* and return the created transaction */

/*--------------------------------------------------------------------*/

TRACT* ta_clone (const TRACT *t)
{ return ta_create(t->items, t->size, t->wgt); }

/*--------------------------------------------------------------------*/

void ta_sort (TRACT *t)
{                               /* --- sort items in transaction */
  int n;                        /* number of items */

  assert(t);                    /* check the function argument */
  if ((n = t->size) < 2) return;/* do not sort less than two items */
  while ((n > 0) && (t->items[n-1] <= TA_END))
    --n;                        /* skip additional end markers */
  int_qsort(t->items, n);       /* and sort the remaining items */
}  /* ta_sort() */

/*--------------------------------------------------------------------*/

void ta_reverse (TRACT *t)
{                               /* --- reverse items in transaction */
  int n;                        /* number of items */

  assert(t);                    /* check the function argument */
  if ((n = t->size) < 2) return;/* do not reverse less than two items */
  while ((n > 0) && (t->items[n-1] <= TA_END))
    --n;                        /* skip additional end markers */
  int_reverse(t->items, n);     /* and reverse the remaining items */
}  /* ta_reverse() */

/*--------------------------------------------------------------------*/

int ta_unique (TRACT *t)
{                               /* --- reverse items in transaction */
  int k, n;                     /* number of items */

  assert(t);                    /* check the function argument */
  if ((n = t->size) < 2)        /* do not process less than two items */
    return n;                   /* (cannot contain duplicates) */
  while ((n > 0) && (t->items[n-1] <= TA_END))
    --n;                        /* skip additional end markers */
  k = int_unique(t->items, n);  /* and remove duplicate items */
  t->size -= n-k;               /* adapt the transaction size */
  while (k < t->size)           /* if necessary, fill again */
    t->items[k++] = TA_END;     /* with additional end markers */
  return t->size;               /* return the new number of items */
}  /* ta_reverse() */

/*--------------------------------------------------------------------*/

int ta_pack (TRACT *t, int n)
{                               /* --- pack items with codes 0--(n-1) */
  int b;                        /* packed items (bit array) */
  int *s, *d, *p;               /* to traverse the items */

  assert(t);                    /* check the function arguments */
  if (n <= 0) return 0;         /* if no items to pack, abort */
  if (n > 31) n = 31;           /* pack at most 31 items */
  for (s = t->items; *s > TA_END; s++)
    if (*s < n) break;          /* find item with code < n */
  if (*s <= TA_END) return 0;   /* if there is none, abort */
  p = d = s;                    /* note the destination location */
  for (b = 0; *s > TA_END; s++) {
    if      (*s < 0) b |= *s;   /* traverse the remaining items */
    else if (*s < n) b |= 1 << *s;
    else             *++d = *s; /* set bits for items with codes < n */
  }                             /* and copy the other items */
  *p = b | TA_END;              /* store the packed items (bit rep.) */
  while (++d < s) *d = TA_END;  /* clear rest of the transaction */
  return b & ~TA_END;           /* return bit rep. of packed items */
}  /* ta_pack() */

/*--------------------------------------------------------------------*/

int ta_unpack (TRACT *t, int dir)
{                               /* --- unpack items (undo ta_pack()) */
  int p, q, i, k;               /* packed items, loop variables */
  int *s, *d;                   /* to traverse the items */

  assert(t);                    /* check the function arguments */
  for (d = t->items; *d >= 0; d++);  /* search for packed items */
  if (*d <= TA_END) return 0;   /* if there are none, abort */
  for (i = k = 0, q = p = *d & ~TA_END; q; q >>= 1) {
    i += q & 1; k++; }          /* get and count the packed items */
  for (s = d+1; *s > TA_END; s++);    /* find end of transaction */
  memmove(d+i, d+1, (s-d) *sizeof(int));  /* move rest of trans. */
  if (dir < 0) {                /* if negative direction requested, */
    for (i = k; --i >= 0; )     /* store items in descending order */
      if (p & (1 << i)) *d++ = i; }
  else {                        /* if positive direction requested, */
    for (i = 0; i < k; i++)     /* store items in ascending order */
      if (p & (1 << i)) *d++ = i; }
  return p & ~TA_END;           /* return bit rep. of packed items */
}  /* ta_unpack() */

/*--------------------------------------------------------------------*/

int ta_equal (const TRACT *t1, const TRACT *t2)
{                               /* --- compare transactions */
  const int *a, *b;             /* to traverse the items */

  assert(t1 && t2);             /* check the function arguments */
  if (t1->size != t2->size)     /* if the sizes differ, */
    return -1;                  /* transactions cannot be equal */
  for (a = t1->items, b = t2->items; *a > TA_END; a++, b++)
    if (*a != *b) return -1;    /* abort if corresp. items differ */
  return 0;                     /* return that trans. are equal */
}  /* ta_equal() */

/*--------------------------------------------------------------------*/

int ta_cmp (const void *p1, const void *p2, void *data)
{                               /* --- compare transactions */
  const int *a, *b;             /* to traverse the items */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const TRACT*)p1)->items;
  b = ((const TRACT*)p2)->items;
  for ( ; 1; a++, b++) {        /* traverse the item arrays */
    if (*a < *b) return -1;     /* compare corresponding items */
    if (*a > *b) return +1;     /* and if one is greater, abort */
    if (*a <= TA_END) return 0; /* otherwise check for the sentinel */
  }                             /* and abort if it is reached */
}  /* ta_cmp() */

/* Note that this comparison function also works correctly if there */
/* are packed items, since packed item entries are always > TA_END. */

/*--------------------------------------------------------------------*/

int ta_cmpep (const void *p1, const void *p2, void *data)
{                               /* --- compare trans. (equal packed) */
  int       i, k;               /* item buffers */
  const int *a, *b;             /* to traverse the items */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const TRACT*)p1)->items;
  b = ((const TRACT*)p2)->items;
  for ( ; 1; a++, b++) {        /* traverse the item arrays */
    i = (*a >= 0) ? *a : 0;     /* get the next items, but */
    k = (*b >= 0) ? *b : 0;     /* use 0 if items are packed */
    if (i < k) return -1;       /* compare corresponding items */
    if (i > k) return +1;       /* and if one is greater, abort */
    if (*a <= TA_END) return 0; /* otherwise check for the sentinel */
  }                             /* and abort if it is reached */
}  /* ta_cmpep() */

/*--------------------------------------------------------------------*/

int ta_cmpoff (const void *p1, const void *p2, void *data)
{                               /* --- compare transactions */
  int i, k;                     /* item buffers */

  assert(p1 && p2);             /* check the function arguments */
  i = ((const TRACT*)p1)->items[*(int*)data];
  k = ((const TRACT*)p2)->items[*(int*)data];
  if (i < k) return -1;         /* compare items at given offset */
  if (i > k) return +1;         /* and if one is greater, abort */
  return 0;                     /* otherwise return 'equal' */
}  /* ta_cmpoff() */

/*--------------------------------------------------------------------*/

int ta_cmplim (const void *p1, const void *p2, void *data)
{                               /* --- compare transactions limited */
  const int *a, *b;             /* to traverse the items */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const TRACT*)p1)->items;
  b = ((const TRACT*)p2)->items;
  for ( ; 1; a++, b++) {        /* traverse the item arrays */
    if (*a < *b) return -1;     /* compare corresponding items */
    if (*a > *b) return +1;     /* and if one is greater, abort */
    if (*a == *(int*)data) return 0;
  }                             /* abort if the limit is reached */
}  /* ta_cmplim() */

/*--------------------------------------------------------------------*/

int ta_cmpsfx (const void *p1, const void *p2, void *data)
{                               /* --- compare transaction suffixes */
  const int *a, *b;             /* to traverse the items */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const TRACT*)p1)->items +*(int*)data;
  b = ((const TRACT*)p2)->items +*(int*)data;
  for ( ; 1; a++, b++) {        /* traverse the item arrays */
    if (*a < *b) return -1;     /* compare corresponding items */
    if (*a > *b) return +1;     /* and if one is greater, abort */
    if (*a <= TA_END) return 0; /* otherwise check for the sentinel */
  }                             /* and abort if it is reached */
}  /* ta_cmpsfx() */

/*--------------------------------------------------------------------*/

int ta_cmpsep (const void *p1, const void *p2, void *data)
{                               /* --- compare transaction suffixes */
  int       i, k;               /* item buffers */
  const int *a, *b;             /* to traverse the items */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const TRACT*)p1)->items +*(int*)data;
  b = ((const TRACT*)p2)->items +*(int*)data;
  for ( ; 1; a++, b++) {        /* traverse the item arrays */
    i = (*a >= 0) ? *a : 0;     /* get the next items, but */
    k = (*b >= 0) ? *b : 0;     /* use 0 if items are packed */
    if (i < k) return -1;       /* compare corresponding items */
    if (i > k) return +1;       /* and if one is greater, abort */
    if (*a <= TA_END) return 0; /* otherwise check for the sentinel */
  }                             /* and abort if it is reached */
}  /* ta_cmpsep() */

/*--------------------------------------------------------------------*/

int ta_cmpx (const TRACT *t, const int *items, int n)
{                               /* --- compare transactions */
  int k;                        /* loop variable */
  const int *i;                 /* to traverse the items */

  assert(t && items);           /* check the function arguments */
  k = (n < t->size) ? n : t->size;
  for (i = t->items; --k >= 0; i++, items++) {
    if (*i < *items) return -1; /* compare corresponding items */
    if (*i > *items) return +1; /* and abort the comparison */
  }                             /* if one of them is greater */
  return t->size -n;            /* return the size difference */
}  /* ta_cmpx() */

/*--------------------------------------------------------------------*/

int ta_cmpsz (const void *p1, const void *p2, void *data)
{                               /* --- compare transactions */
  int a, b;                     /* transaction sizes */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const TRACT*)p1)->size; /* return the sign */
  b = ((const TRACT*)p2)->size; /* of the size difference */
  return (a > b) ? 1 : (a < b) ? -1 : ta_cmp(p1, p2, data);
}  /* ta_cmpsz() */

/*--------------------------------------------------------------------*/

int ta_subset (const TRACT *t1, const TRACT *t2, int off)
{                               /* --- test for subset/subsequence */
  const int *s, *d, *x, *y;     /* to traverse the items */

  assert(t1 && t2 && (off >= 0));  /* check the function arguments */
  if ((off > t2->size) || (t1->size > t2->size -off))
    return -1;                  /* check for (enough) items in dest. */
  s = t1->items;                /* check for empty source sequence, */
  if (*s <= TA_END) return 0;   /* then traverse the destination */
  for (d = t2->items +off; *d > TA_END; d++) {
    if (*d != *s) continue;     /* try to find source start in dest. */
    for (x = s+1, y = d+1; 1; y++) {  /* compare the remaining items */
      if (*x <= TA_END) return d -t2->items;
      if (*y <= TA_END) break;  /* all in source matched, subset, */
      if (*x == *y) x++;        /* end of destination, no subset */
    }                           /* skip matched item in source and */
  }                             /* always skip item in destination */
  return -1;                    /* return 'not subsequence w/o gaps' */
}  /* ta_subset() */

/*--------------------------------------------------------------------*/

int ta_subwog (const TRACT *t1, const TRACT *t2, int off)
{                               /* --- test for subsequence w/o gaps */
  const int *s, *d, *x, *y;     /* to traverse the segments */

  assert(t1 && t2 && (off >= 0));  /* check the function arguments */
  if ((off > t2->size) || (t1->size > t2->size -off))
    return -1;                  /* check for (enough) items in dest. */
  s = t1->items;                /* check for empty source sequence, */
  if (*s <= TA_END) return 0;   /* then traverse the destination */
  for (d = t2->items +off; *d > TA_END; d++) {
    if (*d != *s) continue;     /* try to find source start in dest. */
    x = s; y = d;               /* compare the remaining items */
    do { if (*++x <= TA_END) return d -t2->items; }
    while (*x == *++y);         /* if all items have been found, */
  }                             /* abort with success, otherwise */
  return -1;                    /* return 'not subsequence w/o gaps' */
}  /* ta_subwog() */

/*--------------------------------------------------------------------*/
#ifdef TA_WRITE

int ta_write (const TRACT *t, const ITEMBASE *base,
              TABWRITE *twr, const char *wgtfmt)
{                               /* --- write a transaction */
  int       i, k, p;            /* loop variable, packed items */
  const int *s;                 /* to traverse the items */

  assert(base && twr);          /* check the function arguments */
  if (!t) t = (const TRACT*)base->tract;
  for (k = 0, s = t->items; *s > TA_END; s++) {
    if (*s >= 0) {              /* if unpacked item */
      if (k++ > 0) twr_fldsep(twr);
      twr_puts(twr, ib_name(base, *s)); }
    else {                      /* if packed items */
      p = *s & ~TA_END;         /* get the packed item bits */
      for (i = 0; (1 << i) <= p; i++) {
        if (!(p & (1 << i))) continue;
        if (k++ > 0) twr_fldsep(twr);
        twr_puts(twr, ib_name(base, i));
      }                         /* traverse the bits of the items */
    }                           /* and print the items */
  }                             /* for which the bits are set */
  if (wgtfmt)                   /* print the transaction weight */
    twr_printf(twr, wgtfmt, t->wgt);
  twr_recsep(twr);              /* print a record separator and */
  return twr_error(twr);        /* return a write error indicator */
}  /* ta_write() */

#endif
/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void ta_show (TRACT *t, ITEMBASE *base)
{                               /* --- show a transaction */
  int *i;                       /* to traverse the items */

  for (i = t->items; *i > TA_END; i++) {
    if (*i < 0) { printf("%08x ", *i); continue; }
    if (base) printf("%s/", ib_xname(base, *i));
    printf("%d ", *i);         /* traverse the items and */
  }                            /* print item name and identifier */
  printf("[%d]\n", t->wgt);    /* finally print the weight */
}  /* ta_show() */

#endif
/*----------------------------------------------------------------------
  Weighted Item Functions
----------------------------------------------------------------------*/

int wi_cmp (const WITEM *a, const WITEM *b)
{                               /* --- compare two transactions */
  int i;                        /* loop variable */

  assert(a && b);               /* check the function arguments */
  for (i = 0; 1; i++) {         /* compare the items */
    if (a[i].id > b[i].id) return +1;
    if (a[i].id < b[i].id) return -1;
    if (a[i].id < 0) return 0;  /* check for the sentinel and */
  }                             /* abort if it is reached */
  for (i = 0; 1; i++) {         /* compare the item weights */
    if (a[i].wgt > b[i].wgt) return +1;
    if (a[i].wgt < b[i].wgt) return -1;
    if (a[i].id  < 0) return 0; /* check for the sentinel and */
  }                             /* abort if it is reached */
}  /* wi_cmp() */

/*--------------------------------------------------------------------*/

static void wi_rec (WITEM *wia, int n)
{                               /* --- recursive part of item sorting */
  WITEM *l, *r, t;              /* exchange positions and buffer */
  int   x, m;                   /* pivot element, number of elements */

  do {                          /* sections sort loop */
    l = wia; r = l +n -1;       /* start at left and right boundary */
    if (l->id > r->id) {        /* bring the first and last element */
      t = *l; *l = *r; *r = t;} /* into the proper order */
    x = wia[n >> 1].id;         /* get the middle element as pivot */
    if      (x < l->id) x = l->id;  /* try to find a */
    else if (x > r->id) x = r->id;  /* better pivot element */
    while (1) {                 /* split and exchange loop */
      while ((++l)->id < x);    /* skip smaller elements on the left */
      while ((--r)->id > x);    /* skip greater elements on the right */
      if (l >= r) {             /* if less than two elements left, */
        if (l <= r) { l++; r--; } break; }       /* abort the loop */
      t = *l; *l = *r; *r = t;  /* otherwise exchange elements */
    }
    m = wia +n -l;              /* compute the number of elements */
    n = r -wia +1;              /* right and left of the split */
    if (n > m) {                /* if right section is smaller, */
      if (m >= TH_INSERT)       /* but larger than the threshold, */
        wi_rec(l, m); }         /* sort it by a recursive call, */
    else {                      /* if the left section is smaller, */
      if (n >= TH_INSERT)       /* but larger than the threshold, */
        wi_rec(wia, n);         /* sort it by a recursive call, */
      wia = l; n = m;           /* then switch to the right section */
    }                           /* keeping its size m in variable n */
  } while (n >= TH_INSERT);     /* while greater than threshold */
}  /* wi_rec() */

/*--------------------------------------------------------------------*/

void wi_sort (WITEM *wia, int n)
{                               /* --- sort an transaction item array */
  int   k;                      /* size of first section */
  WITEM *l, *r;                 /* to traverse the array */
  WITEM t;                      /* exchange buffer */

  assert(wia && (n >= 0));      /* check the function arguments */
  if (n <= 1) return;           /* do not sort less than two elements */
  if (n < TH_INSERT)            /* if fewer elements than threshold */
    k = n;                      /* for insertion sort, note the */
  else {                        /* number of elements, otherwise */
    wi_rec(wia, n);             /* call the recursive function */
    k = TH_INSERT -1;           /* and get the number of elements */
  }                             /* in the first array section */
  for (l = r = wia; --k > 0; )  /* find the smallest element */
    if ((++r)->id < l->id) l = r;  /* among the first k elements */
  r = wia;                      /* swap the smallest element */
  t = *l; *l = *r; *r = t;      /* to the front as a sentinel */
  while (--n > 0) {             /* insertion sort loop */
    t = *++r;                   /* note the element to insert */
    for (l = r; (--l)->id > t.id; )   /* shift right elements */
      l[1] = *l;                /* that are greater than the one to */
    l[1] = t;                   /* insert and store the element to */
  }                             /* insert in the place thus found */
}  /* wi_sort() */

/*--------------------------------------------------------------------*/

void wi_reverse (WITEM *wia, int n)
{                               /* --- reverse an item array */
  WITEM t;                      /* exchange buffer */

  assert(wia && (n >= 0));      /* check the function arguments */
  while (--n > 0) {             /* reverse the order of the items */
    t = wia[n]; wia[n--] = wia[0]; *wia++ = t; }
}  /* wi_reverse() */

/*--------------------------------------------------------------------*/

int wi_unique (WITEM *wia, int n)
{                               /* --- make item array unique */
  WITEM *s, *d;                 /* to traverse the item array */

  assert(wia && (n >= 0));      /* check the function arguments */
  if (n <= 1) return n;         /* check for 0 or 1 element */
  for (d = s = wia; --n > 0; ) {
    if      ((++s)->id != d->id) *++d = *s;
    else if (   s->wgt > d->wgt) d->wgt = s->wgt;
  }                             /* collect the unique elements */
  *++d = WTA_END;               /* store a sentinel at the end */
  return d -wia;                /* return the new number of elements */
}  /* wi_unique() */

/*----------------------------------------------------------------------
  Transaction Functions (weighted items)
----------------------------------------------------------------------*/

WTRACT* wta_create (int size, int wgt)
{                               /* --- create a transaction */
  WTRACT *t;                    /* created transaction */

  assert(size >= 0);            /* check the function arguments */
  t = (WTRACT*)malloc(sizeof(WTRACT) +size *sizeof(WITEM));
  if (!t) return NULL;          /* allocate a new transaction */
  t->wgt  = wgt;                /* store the transaction weight */
  t->size = 0;                  /* and init. the transaction size */
  t->items[size] = WTA_END;     /* store a sentinel after the items */
  return t;                     /* return the created transaction */
}  /* wta_create() */

/*--------------------------------------------------------------------*/

WTRACT* wta_clone (const WTRACT *t)
{                               /* --- clone a transaction */
  WTRACT *c;                    /* created transaction clone */
  WITEM  *d; const WITEM *s;    /* to traverse the items */

  c = (WTRACT*)malloc(sizeof(WTRACT) +t->size *sizeof(WITEM));
  if (!c) return NULL;          /* allocate a new transaction */
  c->size = t->size;            /* copy the transaction size */
  c->wgt  = t->wgt;             /* and  the transaction weight */
  for (s = t->items, d = c->items; s->id >= 0; )
    *d++ = *s++;                /* copy the transaction items */
  *d = *s;                      /* copy the sentinel */
  return c;                     /* return the created copy */
}  /* wta_clone() */

/*--------------------------------------------------------------------*/

void wta_add (WTRACT *t, int item, float wgt)
{                               /* --- add an item to a transaction */
  WITEM *d;                     /* destination for the item */

  assert(t && (item >= 0));     /* check the function arguments */
  d = t->items +t->size++;      /* get the next extended item and */
  d->id = item; d->wgt = wgt;   /* store item ientifier and weight */
}  /* wta_add() */

/*--------------------------------------------------------------------*/

void wta_sort (WTRACT *t)
{ wi_sort(t->items, t->size); }

/*--------------------------------------------------------------------*/

void wta_reverse (WTRACT *t)
{ wi_reverse(t->items, t->size); }

/*--------------------------------------------------------------------*/

int wta_unique (WTRACT *t)
{ return t->size = wi_unique(t->items, t->size); }

/*--------------------------------------------------------------------*/

int wta_cmp (const void *p1, const void *p2, void *data)
{                               /* --- compare transactions */
  return wi_cmp(((const WTRACT*)p1)->items,
                ((const WTRACT*)p2)->items);
}  /* wta_cmp() */

/*--------------------------------------------------------------------*/

int wta_cmpsz (const void *p1, const void *p2, void *data)
{                               /* --- compare transactions */
  int a, b;                     /* transaction sizes */

  assert(p1 && p2);             /* check the function arguments */
  a = ((const WTRACT*)p1)->size;/* return the sign */
  b = ((const WTRACT*)p2)->size;/* of the size difference */
  return (a > b) ? 1 : (a < b) ? -1 : wta_cmp(p1, p2, data);
}  /* wta_cmpsz() */

/*--------------------------------------------------------------------*/

int wta_subset (const WTRACT *t1, const WTRACT *t2, int off)
{                               /* --- test for subset/subsequence */
  const WITEM *s, *d, *x, *y;   /* to traverse the items */

  assert(t1 && t2 && (off >= 0));  /* check the function arguments */
  if ((off > t2->size) || (t1->size > t2->size -off))
    return -1;                  /* check for (enough) items in dest. */
  s = t1->items;                /* check for empty source sequence, */
  if (s->id < 0) return 0;      /* then traverse the destination */
  for (d = t2->items +off; d->id >= 0; d++) {
    if (d->id != s->id)         /* try to find source start in dest., */
      continue;                 /* then compare the remaining items */
    for (x = s+1, y = d+1; 1; y++) {
      if (x->id < 0) return d -t2->items;
      if (y->id < 0) break;     /* all in source matched, subset, */
      if (x->id == y->id) x++;  /* end of destination, no subset */
    }                           /* skip matched item in source */
  }                             /* always skip item in destination */
  return -1;                    /* return 'not subsequence w/o gaps' */
}  /* wta_subset() */

/*--------------------------------------------------------------------*/

int wta_subwog (const WTRACT *t1, const WTRACT *t2, int off)
{                               /* --- test for subsequence w/o gaps */
  const WITEM *s, *d, *x, *y;   /* to traverse the segments */

  assert(t1 && t2 && (off >= 0));  /* check the function arguments */
  if ((off > t2->size) || (t1->size > t2->size -off))
    return -1;                  /* check for (enough) items in dest. */
  s = t1->items;                /* check for empty source sequence, */
  if (s->id < 0) return 0;      /* then traverse the destination */
  for (d = t2->items +off; d->id >= 0; d++) {
    if (d->id != s->id)         /* try to find the source start */
      continue;                 /* in the destination sequence */
    x = s; y = d;               /* compare the remaining items */
    do { if ((++x)->id < 0) return d -t2->items; }
    while (x->id == (++y)->id); /* if all items have been found, */
  }                             /* abort with success, otherwise */
  return -1;                    /* return 'not subsequence w/o gaps' */
}  /* wta_subwog() */

/*--------------------------------------------------------------------*/
#ifdef TA_WRITE

int wta_write (const WTRACT *t, const ITEMBASE *base,
               TABWRITE *twr, const char *wgtfmt, const char *iwfmt)
{                               /* --- write a weighted transaction */
  int         k;                /* item counter */
  const WITEM *s;               /* to traverse the items */

  assert(base && twr && iwfmt); /* check the function arguments */
  if (!t) t  = (WTRACT*)base->tract;
  for (k = 0, s = t->items; s->id >= 0; s++) {
    if (k++ > 0) twr_fldsep(twr);  /* print a field separator */
    twr_puts(twr, ib_name(base, s->id));
    twr_printf(twr, iwfmt, s->wgt);
  }                             /* print item and item weight */
  if (wgtfmt)                   /* print the transaction weight */
    twr_printf(twr, wgtfmt, t->wgt);
  twr_recsep(twr);              /* print a record separator and */
  return twr_error(twr);        /* return a write error indicator */
}  /* wta_write() */

#endif
/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void wta_show (WTRACT *t, ITEMBASE *base)
{                               /* --- show an extended transaction */
  int k;                        /* to traverse the items */

  for (k = 0; k < t->size; k++){/* traverse the items */
    if (k > 0) fputc(' ', stdout);
    if (base) printf("%s/", ib_xname(base, t->items[k].id));
    printf("%d:%g", t->items[k].id, t->items[k].wgt);
  }                             /* print item name, id, weight */
  printf(" [%d]\n", t->wgt);    /* finally print the trans. weight */
}  /* wta_show() */

#endif
/*----------------------------------------------------------------------
  Transaction Bag/Multiset Functions
----------------------------------------------------------------------*/

TABAG* tbg_create (ITEMBASE *base)
{                               /* --- create a transaction bag */
  TABAG *bag;                   /* created transaction bag */

  assert(base);                 /* check the function argument */
  bag = (TABAG*)malloc(sizeof(TABAG));
  if (!bag) return NULL;        /* create a transaction bag/multiset */
  if (!base && !(base = ib_create(0,0))) {
    free(bag); return NULL; }   /* create an item base if needed */
  bag->base   = base;           /* store the underlying item base */
  bag->mode   = base->mode;     /* and initialize the other fields */
  bag->extent = bag->wgt  = bag->max = 0;
  bag->cnt    = bag->size = 0;
  bag->tracts = NULL;           /* there are no transactions yet */
  bag->ifrqs  = bag->icnts = NULL;
  return bag;                   /* return the created t.a. bag */
}  /* tbg_create() */

/*--------------------------------------------------------------------*/

void tbg_delete (TABAG *bag, int delis)
{                               /* --- delete a transaction bag */
  assert(bag);                  /* check the function argument */
  if (bag->tracts) {            /* if there are loaded transactions */
    while (--bag->cnt >= 0)     /* traverse the transaction array */
      free(bag->tracts[bag->cnt]);
    free(bag->tracts);          /* delete all transactions */
  }                             /* and the transaction array */
  if (bag->icnts) free (bag->icnts);
  if (delis) ib_delete(bag->base);
  free(bag);                    /* delete the item base and */
}  /* tbg_delete() */           /* the transaction bag body */

/*--------------------------------------------------------------------*/

static int tbg_count (TABAG *bag)
{                               /* --- count item occurrences */
  int    i, k;                  /* loop variables, number of items */
  TRACT  *t;                    /* to traverse the transactions */
  WTRACT *x;                    /* to traverse the transactions */
  int    *s;                    /* to traverse the transaction items */
  WITEM  *p;                    /* to traverse the transaction items */

  k = ib_cnt(bag->base);        /* get the number of items */
  s = (int*)realloc(bag->icnts, (k+k) *sizeof(int));
  if (!s) return -1;            /* allocate the counter arrays */
  bag->icnts = (int*)memset(s, 0, (k+k) *sizeof(int));
  bag->ifrqs = bag->icnts +k;   /* clear and organize them */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    for (k = bag->cnt; --k >= 0; ) {
      x = (WTRACT*)bag->tracts[k]; /* traverse the transactions */
      for (p = x->items; p->id >= 0; p++) {
        bag->icnts[p->id] += 1; /* traverse the transaction items */
        bag->ifrqs[p->id] += x->wgt;
      }                         /* count the occurrences and */
    } }                         /* sum the transaction weights */
  else {                        /* if the items do not carry weights */
    for (k = bag->cnt; --k >= 0; ) {
      t = (TRACT*)bag->tracts[k];  /* traverse the transactions */
      for (s = t->items; *s > TA_END; s++) {
        if ((i = *s) < 0) i = 0;   /* traverse the transaction items */
        bag->icnts[i] += 1;     /* count packed items in 1st element */
        bag->ifrqs[i] += t->wgt;
      }                         /* count the occurrences and */
    }                           /* sum the transaction weights */
  }
  return 0;                     /* return 'ok' */
}  /* tbg_count() */

/*--------------------------------------------------------------------*/

const int* tbg_icnts (TABAG *bag, int recnt)
{                               /* --- number of trans. per item */
  if ((recnt || !bag->icnts) && (tbg_count(bag) < 0)) return NULL;
  return bag->icnts;            /* count the item occurrences */
}  /* tbg_icnts() */

/*--------------------------------------------------------------------*/

const int* tbg_ifrqs (TABAG *bag, int recnt)
{                               /* --- item frequencies (weight) */
  if ((recnt || !bag->ifrqs) && (tbg_count(bag) < 0)) return NULL;
  return bag->ifrqs;            /* determine the item frequencies */
}  /* tbg_ifrqs() */

/*--------------------------------------------------------------------*/

int tbg_add (TABAG *bag, TRACT *t)
{                               /* --- add a standard transaction */
  void **p;                     /* new transaction array */
  int  n;                       /* new transaction array size */

  assert(bag                    /* check the function arguments */
  &&   !(bag->mode & IB_WEIGHTS));
  n = bag->size;                /* get the transaction array size */
  if (bag->cnt >= n) {          /* if the transaction array is full */
    n += (n > BLKSIZE) ? (n >> 1) : BLKSIZE;
    p  = (void**)realloc(bag->tracts, n *sizeof(TRACT*));
    if (!p) return E_NOMEM;     /* enlarge the transaction array */
    bag->tracts = p; bag->size = n;
  }                             /* set the new array and its size */
  if (!t && !(t = ta_clone(ib_tract(bag->base))))
    return E_NOMEM;             /* get trans. from item base if nec. */
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  bag->tracts[bag->cnt++] = t;  /* store the transaction and */
  bag->wgt += t->wgt;           /* sum the transaction weight */
  if (t->size > bag->max) bag->max = t->size;
  bag->extent += t->size;       /* update maximal transaction size */
  return 0;                     /* and return 'ok' */
}  /* tbg_add() */

/*--------------------------------------------------------------------*/

int tbg_addw (TABAG *bag, WTRACT *t)
{                               /* --- add an extended transaction */
  void **p;                     /* new transaction array */
  int  n;                       /* new transaction array size */

  assert(bag                    /* check the function arguments */
  &&    (bag->mode & IB_WEIGHTS));
  n = bag->size;                /* get the transaction array size */
  if (bag->cnt >= n) {          /* if the transaction array is full */
    n += (n > BLKSIZE) ? (n >> 1) : BLKSIZE;
    p  = (void**)realloc(bag->tracts, n *sizeof(WTRACT*));
    if (!p) return E_NOMEM;     /* enlarge the transaction array */
    bag->tracts = p; bag->size = n;
  }                             /* set the new array and its size */
  if (!t && !(t = wta_clone(ib_wtract(bag->base))))
    return E_NOMEM;             /* get trans. from item base if nec. */
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  bag->tracts[bag->cnt++] = t;  /* store the transaction and */
  bag->wgt += t->wgt;           /* sum the transaction weight */
  if (t->size > bag->max) bag->max = t->size;
  bag->extent += t->size;       /* update maximal transaction size */
  return 0;                     /* and return 'ok' */
}  /* tbg_addw() */

/*--------------------------------------------------------------------*/

int tbg_addib (TABAG *bag)
{                               /* --- add transaction from item base */
  assert(bag);                  /* check the function argument */
  return (bag->mode & IB_WEIGHTS) ? tbg_addw(bag, NULL)
                                  : tbg_add (bag, NULL);
}  /* tbg_addib() */

/*--------------------------------------------------------------------*/

int tbg_read (TABAG *bag, TABREAD *tread, int mode)
{                               /* --- read transactions from a file */
  int r;                        /* result of ib_read()/tbg_add() */

  assert(bag && tread);         /* check the function arguments */
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  while (1) {                   /* transaction read loop */
    r = ib_read(bag->base, tread, mode);
    if (r < 0) return r;        /* read the next transaction and */
    if (r > 0) return 0;        /* check for error and end of file */
    r = (bag->mode & IB_WEIGHTS) ? tbg_addw(bag, NULL)
                                 : tbg_add (bag, NULL);
    if (r) return bag->base->err = E_NOMEM;
  }                             /* add transaction to bag/multiset */
}  /* tbg_read() */

/*--------------------------------------------------------------------*/
#ifdef TA_WRITE

int tbg_write (TABAG *bag, TABWRITE *twr, const char *wgtfmt, ...)
{                               /* --- write transactions to a file */
  int        i;                 /* loop variable */
  va_list    args;              /* list of variable arguments */
  const char *iwfmt;            /* item weight format */

  assert(bag && twr);           /* check the function arguments */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    va_start(args, wgtfmt);     /* start variable arguments */
    iwfmt = va_arg(args, const char*);
    for (i = 0; i < bag->cnt; i++)
      wta_write((WTRACT*)bag->tracts[i], bag->base, twr, wgtfmt, iwfmt);
    va_end(args); }
  else {                        /* if the items do not carry weights */
    for (i = 0; i < bag->cnt; i++)
      ta_write ( (TRACT*)bag->tracts[i], bag->base, twr, wgtfmt);
  }                             /* traverse and print transactions */
  return twr_error(twr);        /* return a write error indicator */
}  /* tbg_write() */

#endif
/*--------------------------------------------------------------------*/

void recode (TABAG *bag, int *map)
{                               /* --- recode items in transactions */
  int    i, n;                  /* item buffer, loop variable */
  TRACT  *t;                    /* to traverse the transactions */
  int    *s, *d;                /* to traverse the items */
  WTRACT *x;                    /* to traverse the transactions */
  WITEM  *a, *b;                /* to traverse the items */

  assert(bag && map);           /* check the function arguments */
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  bag->max = bag->extent = 0;   /* clear maximal transaction size */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    for (n = bag->cnt; --n >= 0; ) {
      x = (WTRACT*)bag->tracts[n]; /* traverse the transactions */
      for (a = b = x->items; a->id >= 0; a++) {
        i = map[a->id];         /* traverse and recode the items */
        if (i >= 0) (b++)->id = i; /* remove all items that are */
      }                         /* not mapped (mapped to id < 0) */
      x->size = b -x->items;    /* compute the new number of items */
      x->items[x->size] = WTA_END; /* store a sentinel at the end */
      if (x->size > bag->max)   /* update the maximal trans. size */
        bag->max = x->size;     /* (may differ from the old size */
      bag->extent += x->size;   /* as items may have been removed) */
    } }                         /* and sum the item instances */
  else {                        /* if the items do not carry weights */
    for (n = bag->cnt; --n >= 0; ) {
      t = (TRACT*)bag->tracts[n];   /* traverse the transactions */
      for (s = d = t->items; *s > TA_END; s++) {
        i = map[*s];            /* traverse and recode the items */
        if (i >= 0) *d++ = i;   /* remove all items that are */
      }                         /* not mapped (mapped to id < 0) */
      t->size = d -t->items;    /* compute the new number of items */
      t->items[t->size] = TA_END;  /* store a sentinel at the end */
      if (t->size > bag->max)   /* update the maximal trans. size */
        bag->max = t->size;     /* (may differ from the old size */
      bag->extent += t->size;   /* as items may have been removed) */
    }                           /* and sum the item instances */
  }
}  /* recode() */

/*--------------------------------------------------------------------*/

int tbg_recode (TABAG *bag, int min, int max, int cnt, int dir)
{                               /* --- recode items in transactions */
  int *map;                     /* identifier map for recoding */

  assert(bag);                  /* check the function arguments */
  map = (int*)malloc(ib_cnt(bag->base) *sizeof(int));
  if (!map) return -1;          /* create an item identifier map */
  min = ib_recode(bag->base, min, max, cnt, dir, map);
  recode(bag, map);             /* recode items and transactions */
  free(map);                    /* delete the item identifier map */
  return min;                   /* return the new number of items */
}  /* tbg_recode() */

/*--------------------------------------------------------------------*/

void tbg_filter (TABAG *bag, int min, const int *marks, double wgt)
{                               /* --- filter (items in) transactions */
  int    n;                     /* loop variable */
  TRACT  *t;                    /* to traverse the transactions */
  int    *s, *d;                /* to traverse the items */
  WTRACT *x;                    /* to traverse the transactions */
  WITEM  *a, *b;                /* to traverse the items */

  assert(bag);                  /* check the function arguments */
  if (!marks && (min <= 1)) return;
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  bag->max = bag->extent = 0;   /* clear maximal transaction size */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    for (n = bag->cnt; --n >= 0; ) {
      x = (WTRACT*)bag->tracts[n];  /* traverse the transactions */
      if (marks) {              /* if item markers are given */
        for (a = b = x->items; a->id >= 0; a++)
          if (marks[a->id] && (a->wgt >= wgt)) *b++ = *a;
        x->size = b -x->items;  /* remove unmarked items and */
      }                         /* store the new number of items */
      if (x->size < min)        /* if the transaction is too short, */
        x->size = 0;            /* delete all items (clear size) */
      x->items[x->size] = WTA_END; /* store a sentinel at the end */
      if (x->size > bag->max)   /* update the maximal trans. size */
        bag->max = x->size;     /* (may differ from the old size */
      bag->extent += x->size;   /* as items may have been removed) */
    } }                         /* and sum the item instances */
  else {                        /* if the items do not carry weights */
    for (n = bag->cnt; --n >= 0; ) {
      t = (TRACT*)bag->tracts[n];  /* traverse the transactions */
      if (marks) {              /* if item markers are given */
        for (s = d = t->items; *s > TA_END; s++)
          if (marks[*s]) *d++ = *s;
        t->size = d -t->items;  /* remove unmarked items and */
      }                         /* store the new number of items */
      if (t->size < min)        /* if the transaction is too short, */
        t->size = 0;            /* delete all items (clear size) */
      t->items[t->size] = TA_END;  /* store a sentinel at the end */
      if (t->size > bag->max)   /* update the maximal trans. size */
        bag->max = t->size;     /* (may differ from the old size */
      bag->extent += t->size;   /* as items may have been removed) */
    }                           /* and sum the item instances */
  }
}  /* tbg_filter() */

/*--------------------------------------------------------------------*/

void tbg_trim (TABAG *bag, int min, const int *marks, double wgt)
{                               /* --- trim transactions (sequences) */
  int    k, n;                  /* loop variables */
  TRACT  *t;                    /* to traverse the transactions */
  int    *s, *d;                /* to traverse the items */
  WTRACT *x;                    /* to traverse the transactions */
  WITEM  *a, *b;                /* to traverse the items */

  assert(bag);                  /* check the function arguments */
  bag->max = bag->extent = 0;   /* clear maximal transaction size */
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    for (n = bag->cnt; --n >= 0; ) {
      x = (WTRACT*)bag->tracts[n];  /* traverse the transactions */
      if (marks) {              /* if item markers are given */
        for (a = x->items, k = x->size; --k >= 0; )
          if (marks[a[k].id] && (a[k].wgt >= wgt))
            break;              /* trim items at the end */
        x->size     = ++k;      /* set new sequence length */
        x->items[k] = WTA_END;  /* and a (new) sentinel */
        for (a = b = x->items; a->id >= 0; a++)
          if (marks[a->id] && (a->wgt >= wgt))
            break;              /* trim items at the front */
        if (a > b) {            /* if front items were trimmed */
          while (a->id >= 0) *b++ = *a++;
          x->size = b -x->items;/* move remaining items to the front */
        }                       /* and store the new number of items */
      }
      if (x->size < min)        /* if the transaction is too short, */
        x->size = 0;            /* delete all items (clear size) */
      x->items[x->size] = WTA_END; /* store a sentinel at the end */
      if (x->size > bag->max)   /* update the maximal trans. size */
        bag->max = x->size;     /* (may differ from the old size */
      bag->extent += x->size;   /* as items may have been removed) */
    } }                         /* and sum the item instances */
  else {                        /* if the items do not carry weights */
    for (n = bag->cnt; --n >= 0; ) {
      t = (TRACT*)bag->tracts[n];  /* traverse the transactions */
      if (marks) {              /* if item markers are given */
        for (s = t->items, k = t->size; --k >= 0; )
          if (marks[s[k]]) break;
        t->size     = ++k;      /* trim infrequent items at the end */
        t->items[k] = TA_END;   /* and set a (new) sentinel */
        for (s = d = t->items; *s >= 0; s++)
          if (marks[*s]) break; /* trim infrequent items at the front */
        if (s > d) {            /* if front items were trimmed */
          while (*s >= 0) *d++ = *s++;
          t->size = d -t->items;/* move remaining items to the front */
        }                       /* and store the new number of items */
      }
      if (t->size < min)        /* if the transaction is too short, */
        t->size = 0;            /* delete all items (clear size) */
      t->items[t->size] = TA_END;  /* store a sentinel at the end */
      if (t->size > bag->max)   /* update the maximal trans. size */
        bag->max = t->size;     /* (may differ from the old size */
      bag->extent += t->size;   /* as items may have been removed) */
    }                           /* and sum the item instances */
  }
}  /* tbg_trim() */

/*--------------------------------------------------------------------*/

void tbg_itsort (TABAG *bag, int dir, int heap)
{                               /* --- sort items in transactions */
  int    i, n;                  /* loop variable, number of items */
  TRACT  *t;                    /* to traverse the transactions */
  WTRACT *x;                    /* to traverse the transactions */
  void   (*sortfn)(int*, int);  /* transaction sort function */

  assert(bag);                  /* check the function arguments */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    for (i = bag->cnt; --i >= 0; ) {
      x = (WTRACT*)bag->tracts[i]; /* traverse the transactions */
      wi_sort(x->items, x->size);
      if (dir < 0) wi_reverse(x->items, x->size);
    } }                         /* sort the items in each transaction */
  else {                        /* if the items do not carry weights */
    sortfn = (heap) ? int_heapsort : int_qsort;
    for (i = bag->cnt; --i >= 0; ) {
      t = (TRACT*)bag->tracts[i];  /* traverse the transactions */
      n = t->size;              /* get transaction and its size */
      if (n < 2) continue;      /* do not sort less than two items */
      while ((n > 0) && (t->items[n-1] <= TA_END))
        --n;                    /* skip additional end markers */
      sortfn(t->items, n);      /* sort the items in the transaction */
      if (dir < 0) int_reverse(t->items, n);
    }                           /* reverse the item order */
  }                             /* if the given direction is negative */
}  /* tbg_itsort() */

/*--------------------------------------------------------------------*/

void tbg_mirror (TABAG *bag)
{                               /* --- mirror all transactions */
  int    i;                     /* loop variable */
  TRACT  *t;                    /* to traverse the transactions */
  WTRACT *x;                    /* to traverse the transactions */

  assert(bag);                  /* check the function argument */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    for (i = 0; i < bag->cnt; i++) {
      x = (WTRACT*)bag->tracts[i]; wta_reverse(x); } }
  else {                        /* if the items do not carry weights */
    for (i = 0; i < bag->cnt; i++) {
      t = (TRACT*)bag->tracts[i];   ta_reverse(t); } }
}  /* tbg_mirror() */

/*--------------------------------------------------------------------*/

static void pksort (TRACT **tracts, TRACT **buf, int n, int o)
{                               /* --- sort packed items with binsort */
  int   i, k, x;                /* loop variables, bit pattern */
  int   mask;                   /* overall item bit mask */
  TRACT **dst, **src, **t;      /* to traverse the transactions */
  int   cnts[64];               /* counters for bit pattern occs. */

  assert(tracts && buf);        /* check the function arguments */
  if (n <= 1) return;           /* sort only 2 and more transactions */
  if (n <= 32) {                /* sort few transactions plainly */
    ptr_mrgsort(tracts, n, ta_cmpoff, &o, buf); return; }
  memset(cnts, 0, sizeof(cnts));/* clear the pattern counters */
  for (mask = 0, t = tracts+n; --t >= tracts; ) {
    mask |= x = (*t)->items[o]; /* traverse the transactions, */
    cnts[x & 0x3f]++;           /* combine all bit patterns and */
  }                             /* count patterns with 6 bits */
  src = tracts; dst = buf;      /* get trans. source and destination */
  if (cnts[mask & 0x3f] < n) {  /* if more than one bit pattern */
    for (i = 0; ++i < 64; )     /* traverse the patterns and compute */
      cnts[i] += cnts[i-1];     /* offsets for storing transactions */
    for (t = src+n; --t >= src;)/* sort transactions with bin sort */
      dst[--cnts[(*t)->items[o] & 0x3f]] = *t;
    t = src; src = dst; dst = t;/* exchange source and destination, */
  }                             /* because trans. have been moved */
  for (k = 6; k < 31; k += 5) { /* traverse the remaining sections */
    x = (mask >> k) & 0x1f;     /* if all transactions are zero */
    if (!x) continue;           /* in a section, skip the section */
    memset(cnts, 0, 32*sizeof(int)); /* clear the pattern counters */
    for (t = src+n; --t >= src;)/* count the pattern occurrences */
      cnts[((*t)->items[o] >> k) & 0x1f]++;
    if (cnts[x] >= n) continue; /* check for only one pattern */
    for (i = 0; ++i < 32; )     /* traverse the patterns and compute */
      cnts[i] += cnts[i-1];     /* offsets for storing transactions */
    for (t = src+n; --t >= src;)/* sort transactions with bin sort */
      dst[--cnts[((*t)->items[o] >> k) & 0x1f]] = *t;
    t = src; src = dst; dst = t;/* exchange source and destination, */
  }                             /* because trans. have been moved */
  if (src != tracts)            /* ensure that result is in tracts */
    memcpy(tracts, src, n *sizeof(TRACT*));
}  /* pksort() */

/*--------------------------------------------------------------------*/

static void sort (TRACT **tracts, int n, int o,
                  TRACT **buf, int *cnts, int k, int mask)
{                               /* --- sort trans. with bucket sort */
  int   i, m, x, y;             /* loop variables, item buffers */
  TRACT **t;                    /* to traverse the transactions */

  assert(tracts && buf && cnts);/* check the function arguments */
  if (n <= 16) {                /* if there are only few transactions */
    ptr_mrgsort(tracts,n,(mask > INT_MIN) ?ta_cmpsfx :ta_cmpsep,&o,buf);
    return;                     /* sort the transactions plainly, */
  }                             /* then abort the function */
  memset(cnts-1, 0, (k+1) *sizeof(int));
  x = 0;                        /* clear the transaction counters */
  for (t = tracts+n; --t >= tracts;) {
    x = (*t)->items[o];         /* traverse the transactions */
    if (x < 0) x = (x <= TA_END) ? -1 : 0;
    cnts[x]++;                  /* count the transactions per item */
  }                             /* (0 for packed items, -1 for none) */
  if (cnts[x] >= n) {           /* check for only one or no item */
    if (x < 0) return;          /* if all transactions end, abort */
    x = (*tracts)->items[o];    /* check for packed items */
    if ((x < 0) && (mask <= INT_MIN)) pksort(tracts, buf, n, o);
    sort(tracts, n, o+1, buf, cnts, k, mask);
    if ((x < 0) && (mask >  INT_MIN)) pksort(tracts, buf, n, o);
    return;                     /* sort the whole array recursively */
  }                             /* and then abort the function */
  memcpy(buf, tracts, n *sizeof(TRACT*));
  for (i = 0; i < k; i++)       /* traverse the items and compute */
    cnts[i] += cnts[i-1];       /* offsets for storing transactions */
  for (t = buf+n; --t >= buf; ) {
    x = (*t)->items[o];         /* traverse the transactions again */
    if (x < 0) x = (x <= TA_END) ? -1 : 0;
    tracts[--cnts[x]] = *t;     /* sort w.r.t. the item at offset o */
  }                             /* (0 for packed items, -1 for none) */
  tracts += m = cnts[0];        /* remove trans. that are too short */
  if ((n -= m) <= 0) return;    /* and if no others are left, abort */
  if ((*tracts)->items[o] < 0){ /* if there are packed items, sort */
    pksort(tracts, buf, m = cnts[1] -m, o);
    if (mask <= INT_MIN) {      /* if to treat packed items equally */
      sort(tracts, m, o+1, buf, cnts, k, mask);
      tracts += m;              /* sort suffixes of packed trans. */
      if ((n -= m) <= 0) return;/* and if no other transactions */
    }                           /* are left, abort the function */
  }                             /* traverse the formed sections */
  if ((x = (*tracts)->items[o]) < 0) x &= mask;
  for (t = tracts, i = 0; ++i < n; ) {
    if ((y = (*++t)->items[o]) < 0) y &= mask;
    if (y != x) { x = y;        /* check for start of new section */
      if ((m = t-tracts) > 1)   /* note the new item (new section) */
        sort(tracts, m, o+1, buf, cnts, k, mask);
      tracts = t;               /* sort the section recursively */
    }                           /* and skip the transactions in it */
  }
  if ((m = (t+1)-tracts) > 1)   /* finally sort the last section */
    sort(tracts, m, o+1, buf, cnts, k, mask);
}  /* sort() */

/*--------------------------------------------------------------------*/

void tbg_sort (TABAG *bag, int dir, int mode)
{                               /* --- sort a transaction bag */
  int   n, k;                   /* number of items and transactions */
  int   mask;                   /* mask for packed item treatment */
  TRACT **buf;                  /* trans. buffer for bucket sort */
  int   *cnts;                  /* counter array for bin sort */
  CMPFN *cmp;                   /* comparison function */

  assert(bag);                  /* check the function arguments */
  if (bag->cnt < 2) return;     /* check for at least two trans. */
  n = bag->cnt;                 /* get the number of transactions */
  k = ib_cnt(bag->base);        /* and the number of items */
  if (k < 2) k = 2;             /* need at least 2 counters */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    if (mode & TA_HEAP) ptr_heapsort(bag->tracts, n, wta_cmp, NULL);
    else                ptr_qsort   (bag->tracts, n, wta_cmp, NULL); }
  else if ((buf = (TRACT**)malloc(n*sizeof(TRACT*)+(k+1)*sizeof(int)))){
    if (k < n) {                /* if bin sort is possible/favorable, */
      cnts = (int*)(buf+n)+1;   /* use bin sort to sort transactions */
      mask = (mode & TA_EQPACK) ? INT_MIN : -1;
      sort((TRACT**)bag->tracts, n, 0, buf, cnts, k, mask); }
    else {                      /* if more items than transactions */
      cmp = (mode & TA_EQPACK) ? ta_cmpep : ta_cmp;
      ptr_mrgsort(bag->tracts, n, cmp, NULL, buf);
    }                           /* sort transactions with merge sort */
    free(buf); }                /* delete the allocated buffer */
  else {                        /* if to use failsafe functions */
    cmp = (mode & TA_EQPACK) ? ta_cmpep : ta_cmp;
    if (mode & TA_HEAP) ptr_heapsort(bag->tracts, n, cmp, NULL);
    else                ptr_qsort   (bag->tracts, n, cmp, NULL);
  }                             /* sort the transactions */
  if (dir < 0) ptr_reverse(bag->tracts, n);
}  /* tbg_sort() */             /* if necessary, reverse the order */

/*--------------------------------------------------------------------*/

void tbg_sortsz (TABAG *bag, int dir, int mode)
{                               /* --- sort a transaction bag */
  int k;                        /* number of transactions */

  assert(bag);                  /* check the function arguments */
  k = bag->cnt;                 /* get the number of transactions */
  if (bag->mode & IB_WEIGHTS) { /* if the items carry weights */
    if (mode & TA_HEAP) ptr_heapsort(bag->tracts, k, wta_cmpsz, NULL);
    else                ptr_qsort   (bag->tracts, k, wta_cmpsz, NULL); }
  else {                        /* if the items do not carry weights */
    if (mode & TA_HEAP) ptr_heapsort(bag->tracts, k,  ta_cmpsz, NULL);
    else                ptr_qsort   (bag->tracts, k,  ta_cmpsz, NULL);
  }                             /* sort the transactions */
  if (dir < 0) ptr_reverse(bag->tracts, bag->cnt);
}  /* tbg_sortsz() */           /* if necessary, reverse the order */

/*--------------------------------------------------------------------*/

int tbg_reduce (TABAG *bag, int keep0)
{                               /* --- reduce a transaction bag */
  /* This function presupposes that the transaction bag has been */
  /* sorted with one of the above sorting functions beforehand.  */
  int   i, c;                   /* loop variable, comparison result */
  TRACT **s, **d;               /* to traverse the transactions */

  assert(bag);                  /* check the function argument */
  if (bag->cnt <= 1) return 1;  /* deal only with two or more trans. */
  if (bag->icnts) {             /* delete the item-specific counters */
    free(bag->icnts); bag->ifrqs = bag->icnts = NULL; }
  bag->extent = 0;              /* reinit. number of item occurrences */
  s = d = (TRACT**)bag->tracts; /* traverse the sorted transactions */
  for (i = bag->cnt; --i > 0; ) {
    c = (*++s)->size -(*d)->size;     /* compare the size first and */
    if (c == 0)                 /* compare items only for same size */
      c = (bag->mode & IB_WEIGHTS) ? wta_cmp(*s, *d, NULL)
                                   :  ta_cmp(*s, *d, NULL);
    if (c == 0) {               /* if the transactions are equal */
      (*d)->wgt += (*s)->wgt;   /* combine the transactions */
      free(*s); }               /* by summing their weights */
    else {                      /* if transactions are not equal */
      if (keep0 || ((*d)->wgt != 0))
           bag->extent += (*d++)->size;
      else free(*d);            /* check weight of old transaction */
      *d = *s;                  /* copy the new transaction */
    }                           /* to close a possible gap */
  }                             /* (collect unique transactions) */
  if (keep0 || ((*d)->wgt != 0))
       bag->extent += (*d++)->size;
  else free(*d);                /* check weight of last transaction */
  return bag->cnt = d -(TRACT**)bag->tracts;
}  /* tbg_reduce() */           /* return new number of transactions */

/*--------------------------------------------------------------------*/

void tbg_pack (TABAG *bag, int n)
{                               /* --- pack all transactions */
  int i;                        /* loop variable */

  assert(bag                    /* check the function arguments */
  &&   !(bag->mode & IB_WEIGHTS));
  if (n <= 0) return;           /* if no items to pack, abort */
  for (i = 0; i < bag->cnt; i++)/* pack items in all transactions */
    ta_pack((TRACT*)bag->tracts[i], n);
  bag->mode |= n & TA_PACKED;   /* set flag for packed transactions */
}  /* tbg_pack() */

/*--------------------------------------------------------------------*/

void tbg_unpack (TABAG *bag, int dir)
{                               /* --- unpack all transactions */
  int i;                        /* loop variable */

  assert(bag                    /* check the function arguments */
  &&   !(bag->mode & IB_WEIGHTS));
  for (i = 0; i < bag->cnt; i++)/* pack items in all transactions */
    ta_unpack((TRACT*)bag->tracts[i], dir);
  bag->mode &= ~TA_PACKED;      /* clear flag for packed transactions */
}  /* tbg_unpack() */

/*--------------------------------------------------------------------*/

int tbg_occur (TABAG *bag, const int *items, int n)
{                               /* --- count transaction occurrences */
  int l, r, m, k;               /* index and loop variables */

  assert(bag && items           /* check the function arguments */
  &&   !(bag->mode & IB_WEIGHTS));
  k = bag->cnt;                 /* get the number of transactions */
  for (r = m = 0; r < k; ) {    /* find right boundary */
    m = (r+k) >> 1;             /* by a binary search */
    if (ta_cmpx((TRACT*)bag->tracts[m], items, n) > 0) k = m;
    else                                               r = m+1;
  }
  for (l = m = 0; l < k; ) {    /* find left boundary */
    m = (l+k) >> 1;             /* by a binary search */
    if (ta_cmpx((TRACT*)bag->tracts[m], items, n) < 0) l = m+1;
    else                                               k = m;
  }
  for (k = 0; l < r; l++)       /* traverse the found section */
    k += tbg_tract(bag,l)->wgt; /* sum the transaction weights */
  return k;                     /* return the number of occurrences */
}  /* tbg_occur() */

/*--------------------------------------------------------------------*/

int tbg_ipwgt (TABAG *bag, int mode)
{                               /* --- compute idempotent weights */
  /* Requires the transactions to be sorted ascendingly by size and */
  /* reduced to unique occurrences (tbg_sortsz() and tbg_reduce()). */
  int       i, k, n, r;         /* loop variables, buffers */
  double    w, sum;             /* (sum of) transaction weights */
  void      ***occs, **o;       /* item occurrence arrays */
  TRACT     *s, *d;             /* to traverse the transactions */
  const int *p;                 /* to traverse the items */
  SUBFN     *sub;               /* subset/subsequence function */
  WTRACT    *x, *y;             /* to traverse the transactions */
  WITEM     *z;                 /* to traverse the items */
  SUBWFN    *subw;              /* subset/subsequence function */
  double    *wgts;              /* buffer for item weight adaptation */

  assert(bag);                  /* check the function argument */
  wgts = NULL;                  /* if to adapt item weights */
  if ((bag->mode & mode & IB_WEIGHTS)
  &&  (mode & TA_NOGAPS) && (mode & TA_ALLOCC)
  && !(wgts = (double*)malloc(bag->max *sizeof(double))))
    return -1;                  /* allocate a weight array */
  p = tbg_icnts(bag, 0);        /* get the occurrences per item */
  k = tbg_itemcnt(bag);         /* get the number of items and */
  n = tbg_extent(bag);          /* the total item occurrences */
  occs = (!p) ? NULL            /* allocate the transaction arrays */
       : (void***)malloc(k*sizeof(TRACT**) +(n+k)*sizeof(TRACT*));      
  if (occs) {                   /* if allocation succeeded */
    o = (void**)(occs +k);      /* organize the memory and */
    for (i = 0; i < k; i++) {   /* store a sentinel at the end */
      occs[i] = o += p[i]; *o++ = NULL; }
    if (bag->mode & IB_WEIGHTS){/* if the items carry weights */
      for (i = 0; i < bag->cnt; i++) {
        x = tbg_wtract(bag, i); /* traverse the transactions */
        for (z = x->items; z->id >= 0; z++)
          if ((WTRACT*)*occs[z->id] != x)
            *--occs[z->id] = x; /* collect containing transactions */
      }                         /* per item, but avoid duplicates */
      subw = (mode & TA_NOGAPS) ? wta_subwog : wta_subset;
      for (i = bag->cnt-1; --i >= 0; ) {
        x = tbg_wtract(bag, i); /* traverse the transactions */
        if ((k = x->items[0].id) < 0) {
          x->wgt -= bag->wgt -x->wgt;
          continue;             /* if the transaction is empty, */
        }                       /* it can be handled directly */
        if (wgts) memset(wgts, 0, x->size *sizeof(double));
        sum = 0;                /* init. the item weight array */
        for (o = occs[k]; (y = (WTRACT*)*o) != x; o++) {
          if (y->size <= x->size)
            continue;           /* traverse larger transactions */
          for (r = -1; (r = subw(x, y, r+1)) >= 0; ) {
            x->wgt -= y->wgt;   /* update the transaction weight */
            if (!wgts) {        /* skip item weights if not needed */
              if (!(mode & TA_ALLOCC)) break; continue; }
            sum += w = (y->wgt != 0) ? y->wgt : 1;
            for (z = y->items +r, n = 0; n < x->size; n++)
              wgts[n] += w *(double)z[n].wgt;
          }                     /* sum the item weight contributions */
        }                       /* from supersets or supersequences */
        if (!wgts) continue;    /* skip item weights if not needed */
        sum += w = (x->wgt != 0) ? x->wgt : 1;
        for (z = x->items, n = 0; n < x->size; n++)
          z[n].wgt = (float)((sum *(double)z[n].wgt -wgts[n]) /w);
      } }                       /* rescale the item weights */
    else {                      /* if the items do not carry weights */
      for (i = 0; i < bag->cnt; i++) {
        s = tbg_tract(bag, i);  /* traverse the transactions */
        for (p = s->items; *p >= 0; p++)
          if ((TRACT*)*occs[*p] != s)
            *--occs[*p] = s;    /* collect containing transactions */
      }                         /* per item, but avoid duplicates */
      sub = (mode & TA_NOGAPS) ? ta_subwog : ta_subset;
      for (i = bag->cnt-1; --i >= 0; ) {
        s = tbg_tract(bag, i);  /* traverse the transactions */
        if ((k = s->items[0]) <= TA_END) {
          s->wgt -= bag->wgt -s->wgt;
          continue;             /* if the transaction is empty, */
        }                       /* it can be handled directly */
        for (o = occs[k]; (d = (TRACT*)*o) != s; o++) {
          if (d->size <= s->size) continue;
          for (r = -1; (r = sub(s, d, r+1)) >= 0; ) {
            s->wgt -= d->wgt;   /* traverse transaction pairs and */
            if (!(mode & TA_ALLOCC)) break;
          }                     /* subtract transaction weights */
        }                       /* of supersets or supersequences */
      }                         /* stop after first occurrence */
    }                           /* unless all occurrences requested */
    free(occs); }               /* deallocate working memory */
  else {                        /* failsafe, but slower variant */
    if (bag->mode & IB_WEIGHTS){/* if the items carry weights */
      subw = (mode & TA_NOGAPS) ? wta_subwog : wta_subset;
      for (i = bag->cnt-1; --i >= 0; ) {
        x = tbg_wtract(bag, i); /* traverse the transactions */
        if ((k = x->items[0].id) < 0) {
          x->wgt -= bag->wgt -x->wgt;
          continue;             /* if the transaction is empty, */
        }                       /* it can be handled directly */
        if (wgts) memset(wgts, 0, x->size *sizeof(double));
        sum = 0;                /* init. the item weight array */
        for (k = bag->cnt; --k > i; ) {
          y = tbg_wtract(bag,k);/* traverse larger transactions */
          if (y->size <= x->size) continue;
          for (r = -1; (r = subw(x, y, r+1)) >= 0; ) {
            x->wgt -= y->wgt;   /* update the transaction weight */
            if (!wgts) {        /* skip item weights if not needed */
              if (!(mode & TA_ALLOCC)) break; continue; }
            sum += w = (y->wgt != 0) ? y->wgt : 1;
            for (z = y->items +r, n = 0; n < x->size; n++)
              wgts[n] += w *(double)z[n].wgt;
          }                     /* sum the item weight contributions */
        }                       /* from supersets or supersequences */
        if (!wgts) continue;    /* skip item weights if not needed */
        sum += w = (x->wgt != 0) ? x->wgt : 1;
        for (z = x->items, n = 0; n < x->size; n++)
          z[n].wgt = (float)((sum *(double)z[n].wgt -wgts[n]) /w);
      } }                       /* rescale the item weights */
    else {                      /* if the items do not carry weights */
      sub = (mode & TA_NOGAPS) ? ta_subwog : ta_subset;
      for (i = bag->cnt-1; --i >= 0; ) {
        s = tbg_tract(bag, i);  /* traverse the transactions */
        if ((k = s->items[0]) <= TA_END) {
          s->wgt -= bag->wgt -s->wgt;
          continue;             /* if the transaction is empty, */
        }                       /* it can be handled directly */
        for (k = bag->cnt; --k > i; ) {
          d = tbg_tract(bag,k); /* traverse larger transactions */
          if (d->size <= s->size) continue;
          for (r = 1; (r = sub(s, d, r+1)) >= 0; ) {
            s->wgt -= d->wgt;   /* traverse transaction pairs and */
            if (!(mode & TA_ALLOCC)) break;
          }                     /* if a transactions is a subset or */
        }                       /* a subsequence of another trans., */
      }                         /* subtract the transaction weight */
    }                           /* stop after first occurrence */
  }                             /* unless all occurrences requested */
  if (wgts) free(wgts);         /* deallocate working memory */
  return 0;                     /* return 'ok' */
}  /* tbg_ipwgt() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void tbg_show (TABAG *bag)
{                               /* --- show a transaction bag */
  int i;                        /* loop variable */

  assert(bag);                  /* check the function argument */
  for (i = 0; i < bag->cnt; i++) {
    printf("%5d: ", i);         /* traverse the transactions */
    if (bag->mode & IB_WEIGHTS) /* and print a transaction id */
         wta_show((WTRACT*)bag->tracts[i], bag->base);
    else  ta_show( (TRACT*)bag->tracts[i], bag->base);
  }                             /* print the transactions */
  printf("%d/%d transaction(s)\n", bag->cnt, bag->wgt);
}  /* tbg_show() */

#endif
/*----------------------------------------------------------------------
  Transaction Array Functions
----------------------------------------------------------------------*/

void taa_collate (TRACT **taa, int n, int k)
{                               /* --- collate transactions */
  int   i, x, y;                /* loop variable, buffers */
  TRACT *s, *d;                 /* to traverse the transactions */
  int   *a, *b;                 /* to traverse the items */

  assert(taa && (n >= 0));      /* check the function arguments */
  for (d = *taa, i = 0; ++i < n; ) {
    s = taa[i];                 /* traverse the transactions */
    a = d->items; b = s->items; /* compare packed items */
    x = (ispacked(*a)) ? *a++ : 0;
    y = (ispacked(*b)) ? *b++ : 0;
    if (x != y) { d = s; continue; }
    for ( ; (unsigned int)*a < (unsigned int)k; a++, b++)
      if (*a != *b) break;      /* compare transaction prefixes */
    if (*a != k) d = s;         /* if not equal, keep transaction */
    else d->wgt -= s->wgt = -s->wgt;
  }                             /* otherwise combine trans. weights */
}  /* taa_collate() */

/*--------------------------------------------------------------------*/

void taa_uncoll (TRACT **taa, int n)
{                               /* --- uncollate transactions */
  int   i;                      /* loop variable */
  TRACT *s, *d;                 /* to traverse the transactions */

  assert(taa && (n >= 0));      /* check the function arguments */
  for (d = *taa, i = 0; ++i < n; ) {
    s = taa[i];                 /* traverse the transactions */
    if (s->wgt >= 0) d = s;     /* get uncollated transactions */
    else d->wgt -= s->wgt = -s->wgt;
  }                             /* uncombine the trans. weights */
}  /* taa_uncoll() */

/*--------------------------------------------------------------------*/

int taa_tabsize (int n)
{                               /* --- compute hash table size */
  #if 1                         /* smaller table, but more collisions */
  n = (n < 2*(INT_MAX/3)) ? n+(n >> 1) : n;
  #else                         /* larger  table, but less collisions */
  n = (n <   (INT_MAX/2)) ? n+n        : n;
  #endif
  n = int_bsearch(n, primes, TS_PRIMES);
  if (n < 0) n = -1-n;          /* find next larger size */
  return primes[(n >= TS_PRIMES) ? TS_PRIMES-1 : n];
}  /* taa_tabsize() */

/*--------------------------------------------------------------------*/
#if 0                           /* reduction with sorting */

int taa_reduce (TRACT **taa, int n, int end,
                const int *map, void *buf, void **dst)
{                               /* --- reduce a transaction array */
  int   i, x;                   /* loop variable, buffer */
  TRACT *t, **p;                /* to traverse the transactions */
  int   *s, *d;                 /* to traverse the items */

  assert(taa                    /* check the function arguments */
  &&    (n >= 0) && (end >= 0) && map && buf && dst && *dst);
  t = *(TRACT**)dst; p = taa;   /* get the transaction memory */
  for (i = 0; i < n; i++) {     /* traverse the transactions */
    s = taa[i]->items; d = t->items;
    if (ispacked(*s)) {         /* if there are packed items, */
      x = *s++ & map[0];        /* remove those not present in mask */
      if (x) *d++ = x | TA_END; /* if there are packed items left, */
    }                           /* store them in the destination */
    for ( ; (unsigned int)*s < (unsigned int)end; s++)
      if ((x = map[*s]) >= 0)   /* map/remove the transaction items */
        *d++ = x;               /* and store them in the destination */
    t->size = d -t->items;      /* compute the size of the trans. */
    if (t->size <= 0) continue; /* delete empty transactions */
    int_qsort(t->items,t->size);/* sort the collected items */
    t->wgt = taa[i]->wgt;       /* copy the transaction weight */
    *d++ = TA_END;              /* store a sentinel at the end */
    *p++ = t; t = (TRACT*)d;    /* store the filtered transaction */
  }                             /* and get the next destination */
  n = p -taa;                   /* get new number of transactions */
  if (n >= 32) {                /* if there are enough trans. left */
    if (end >= n) ptr_mrgsort(taa, n, ta_cmp, NULL, buf);
    else sort(taa, n, 0, buf, (int*)((TRACT*)buf+n)+1, end, -1);
  }                             /* sort the reduced transactions */
  for (p = taa, i = 0; ++i < n; ) {
    t = taa[i];                 /* traverse the sorted transactions */
    if (t->size != (*p)->size){ /* compare the trans. size first */
      *++p = t; continue; }     /* and items only for same size */
    for (s = t->items, d = (*p)->items; 1; s++, d++) {
      if (*s != *d)     { *++p = t;            break; }
      if (*s <= TA_END) { (*p)->wgt += t->wgt; break; }
    }                           /* copy unequal transactions and */
  }                             /* combine equal transactions */
  return p+1 -taa;              /* return the new number of trans. */
}  /* taa_reduce() */

/*--------------------------------------------------------------------*/
#else                           /* reduction with hash table */

int taa_reduce (TRACT **taa, int n, int end,
                const int *map, void *buf, void **dst)
{                               /* --- reduce a transaction array */
  int   i, m, x;                /* loop variable, buffer, extent */
  unsigned int h, k, y, z;      /* hash value/index and table size */
  TRACT *t, *u, **p;            /* to traverse the transactions */
  int   *s, *d;                 /* to traverse the items */
  TRACT **htab = buf;           /* hash table for reduction */

  assert(taa                    /* check the function arguments */
  &&    (n > 0) && (end > 0) && map && buf && dst && *dst);
  z = taa_tabsize(n);           /* get the hash table size */
  t = *(TRACT**)dst;            /* and the transaction memory */
  for (i = x = 0; i < n; i++) { /* traverse the transactions */
    s = taa[i]->items; d = t->items;
    if (ispacked(*s)) {         /* if there are packed items, */
      m = *s++ & map[0];        /* remove those not present in mask */
      if (m) *d++ = m | TA_END; /* if there are packed items left, */
    }                           /* store them in the destination */
    for ( ; (unsigned int)*s < (unsigned int)end; s++)
      if ((m = map[*s]) >= 0)   /* map/remove the transaction items */
        *d++ = m;               /* and store them in the destination */
    t->size = d -t->items;      /* compute the size of the trans. */
    if (t->size <= 0) continue; /* ignore empty transactions */
    int_qsort(t->items,t->size);/* sort the collected items */
    for (h = *(s = t->items); ++s < d; )
      h = h *16777619 +*s;      /* compute the hash value and */
    *d++ = h;                   /* store it in the transaction */
    y = h % (z-2) +1;           /* compute probing step width */
    for (k = h % z; htab[k]; k = (k+y) % z) {
      u = htab[k];              /* search transaction in hash table */
      if ((u->size != t->size)  /* if collision with diff. trans. */
      ||  (u->items[u->size] != h))
        continue;               /* skip the hash bin */
      for (m = t->size; --m >= 0; )
        if (t->items[m] != u->items[m]) break;
      if (m < 0) break;         /* if collision with same trans., */
    }                           /* abort probing (can combine) */
    if (htab[k]) htab[k]->wgt += taa[i]->wgt;
    else { htab[k] = t; t->wgt = taa[i]->wgt;
           x += t->size; t = (TRACT*)d; }
  }                             /* store transaction in hash table */
  for (p = taa, k = 0; k < z; k++) {
    if (!(t = htab[k])) continue;
    htab[k] = NULL;             /* traverse the hash table, */
    t->items[t->size] = TA_END; /* store a sentinel after the items */
    *p++ = t;                   /* and collect the transactions */
  }                             /* (unique occurrences) */
  n = p -taa;                   /* get the new number of tuples */
  *dst = realloc(*dst, taa_dstsize(n, x));
  return n;                     /* return the new number of trans. */
}  /* taa_reduce() */

#endif
/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void taa_show (TRACT **taa, int n, ITEMBASE *base)
{                               /* --- show a transaction array */
  assert(taa && (n >= 0));      /* check the function arguments */
  while (--n >= 0)              /* traverse the array and */
    ta_show(*taa++, base);      /* show each transaction */
}  /* taa_show() */

#endif
/*----------------------------------------------------------------------
  Transaction Tree Functions
----------------------------------------------------------------------*/
#ifdef TATREEFN
#ifdef TATCOMPACT

int create (TANODE *node, TRACT **tracts, int cnt, int index)
{                               /* --- recursive part of tat_create() */
  int    i, k;                  /* loop variables, buffer */
  int    item, n;               /* item and item counter */
  TANODE *child;                /* created sibling node array */

  assert(tracts                 /* check the function arguments */
  &&    (cnt > 0) && (index >= 0));
  if (cnt <= 1) {               /* if only one transaction left */
    node->wgt  = (*tracts)->wgt;/* store weight and suffix length */
    node->max  = -((*tracts)->size -index);
    node->data = (*tracts)->items +index;
    return 0;                   /* store the transaction suffix and */
  }                             /* abort the function with success */
  node->wgt = node->max = 0;    /* init. weight and suffix length */
  while ((--cnt >= 0) && ((*tracts)->size <= index))
    node->wgt += (*tracts++)->wgt; /* skip trans. that are too short */
  for (n = 0, item = TA_END, i = cnt; i >= 0; --i) {
    node->wgt += tracts[i]->wgt;   /* traverse the transactions */
    k = tracts[i]->items[index];   /* and sum their weights */
    if (k != item) { item = k; n++; }
  }                             /* count the different items */
  child = (TANODE*)malloc(n *sizeof(TANODE) +sizeof(int));
  if (!child) { node->data = NULL; return -1; }
  child[n].item = TA_END;       /* create a child node array and */
  node->data = child;           /* store a sentinel at the end, */
  node->max  = 1;               /* then store it in the parent node */
  for ( ; cnt >= 0; cnt = i) {  /* while there are transactions left */
    child->item = tracts[cnt]->items[index];
    for (i = cnt; i >= 0; i--)  /* find range of the next item */
      if (tracts[i]->items[index] != child->item) break;
    if (_create(child, tracts+i+1, cnt-i, index+1) != 0) {
      free(node->data); node->data = NULL;
      node->max = 0; return -1; /* recursively fill the child nodes */
    }                           /* and on error clean up and abort */
    if ((k = abs(child->max) +1) > node->max) node->max = k;
    child++;                    /* update the maximal suffix length */
  }                             /* and go to the next child node */
  return 0;                     /* return 'ok' */
}  /* create() */

/*--------------------------------------------------------------------*/

TATREE* tat_create (TABAG *bag)
{                               /* --- create a transactions tree */
  TATREE *tree;                 /* created transaction tree */

  assert(bag);                  /* check the function argument */
  tree = (TATREE*)malloc(sizeof(TATREE));
  if (!tree) return NULL;       /* create the transaction tree body */
  tree->bag = bag;              /* note the underlying item set */
  if (bag->cnt <= 0) {          /* if the transaction bag is empty */
    tree->root.max  = tree->root.wgt = 0;
    tree->root.data = tree->suffix; }  /* store empty trans. suffix */
  else {                        /* if the bag contains transactions */
    if (_create(&tree->root, (TRACT**)bag->tracts, bag->cnt, 0) != 0) {
      tat_delete(tree, 0); return NULL; }
  }                             /* recursively build the tree */
  tree->root.item = -1;         /* root node represents no item */
  tree->suffix[0] = TA_END;     /* init. the empty trans. suffix */
  return tree;                  /* return the created tree */
}  /* tat_create() */

/*--------------------------------------------------------------------*/

void delete (TANODE *node)
{                               /* --- delete a transaction (sub)tree */
  TANODE *child;                /* to traverse the child nodes */

  assert(node && (node->max > 0)); /* check the function argument */
  for (child = (TANODE*)node->data; child->item >= 0; child++)
    if (child->max > 0) delete(child);
  free(node->data);             /* recursively delete the subtree */
}  /* delete() */

/*--------------------------------------------------------------------*/

void tat_delete (TATREE *tree, int del)
{                               /* --- delete a transaction tree */
  assert(tree);                 /* check the function argument */
  if (tree->root.max > 0)       /* if the root has children, */
    delete(&tree->root);        /* delete the nodes of the tree */
  if (tree->bag && del)         /* delete the transaction bag */
    tbg_delete(tree->bag, (del > 1));
  free(tree);                   /* delete the transaction tree body */
}  /* tat_delete() */

/*--------------------------------------------------------------------*/

static int nodecnt (const TANODE *node)
{                               /* --- count the nodes */
  int    n = 1;                 /* node counter */
  TANODE *child;                /* to traverse the child nodes */

  assert(node && (node->max > 0)); /* check the function argument */
  for (child = (TANODE*)node->data; child->item >= 0; child++)
    if (child->max > 0) n += nodecnt(child);
  return n;                     /* recursively count the nodes */
}  /* nodecnt() */              /* return number of nodes in tree */

/*--------------------------------------------------------------------*/

int tat_size (const TATREE *tree)
{ return (tree->root.max > 0) ? nodecnt(&tree->root) : 1; }

/*--------------------------------------------------------------------*/

int tat_filter (TATREE *tree, int min, const int *marks, int heap)
{                               /* --- filter a transaction tree */
  TABAG *bag;                   /* underlying transaction bag */

  assert(tree);                 /* check the function argument */
  delete(&tree->root);          /* delete the nodes of the tree */
  tbg_filter(bag = tree->bag, min, marks, 0);
  tbg_sort  (bag, 0, heap);     /* remove unnec. items and trans. */
  tbg_reduce(bag, 0);           /* and reduce trans. to unique ones */
  if (create(&tree->root, (TRACT**)bag->tracts, bag->cnt, 0) == 0)
    return 0;                   /* recreate the transaction tree */
  delete(&tree->root);          /* on failure delete the nodes */
  tree->root.max  = tree->root.wgt = 0;
  tree->root.data = tree->suffix;
  return -1;                    /* return an error indicator */
}  /* tat_filter() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

static void show (TANODE *node, ITEMBASE *base, int ind)
{                               /* --- rekursive part of tat_show() */
  int    i, k;                  /* loop variables */
  TANODE **cnds;                /* array of child nodes */

  assert(node && (ind >= 0));   /* check the function arguments */
  if (node->size <= 0) {        /* if this is a leaf node */
    for (i = 0; i < node->max; i++)
      printf("%s ", ib_xname(base, node->items[i]));
    printf("[%d]\n", node->wgt);
    return;                     /* print the items in the */
  }                             /* (rest of) the transaction */
  cnds = TAN_CHILDREN(node);    /* get the child pointer array */
  for (i = 0; i < node->size; i++) {
    if (i > 0) for (k = ind; --k >= 0; ) printf("  ");
    printf("%s ", ib_xname(base, node->items[i]));
    show(cnds[i], base, ind+1); /* traverse the items, print them, */
  }                             /* and show the children recursively */
}  /* show() */

/*--------------------------------------------------------------------*/

void tat_show (TATREE *tree)
{                               /* --- show a transaction tree */
  assert(tree);                 /* check the function argument */
  show(tree->root, tbg_base(tree->bag), 0);
}  /* tat_show() */              /* call the recursive function */

#endif
/*--------------------------------------------------------------------*/
#else  /* #ifdef TATCOMPACT */

void delete (TANODE *root)
{                               /* --- delete a transaction (sub)tree */
  int    i;                     /* loop variable */
  TANODE **cnds;                /* array of child nodes */

  assert(root);                 /* check the function argument */
  cnds = TAN_CHILDREN(root);    /* get the child pointer array */
  for (i = root->size; --i >= 0; )
    delete(cnds[i]);            /* recursively delete the subtrees */
  free(root);                   /* and the tree node itself */
}  /* delete() */

/*--------------------------------------------------------------------*/

TANODE* create (TRACT **tracts, int cnt, int index)
{                               /* --- recursive part of tat_create() */
  int    i, k, w;               /* loop variables, buffers */
  int    item, n;               /* item and item counter */
  TANODE *node;                 /* node of created transaction tree */
  TANODE **cnds;                /* array of child nodes */

  assert(tracts                 /* check the function arguments */
  &&    (cnt > 0) && (index >= 0));
  if (cnt <= 1) {               /* if only one transaction left */
    n    = (*tracts)->size -index;
    node = (TANODE*)malloc(sizeof(TANODE) +n *sizeof(int));
    if (!node) return NULL;     /* create a transaction tree node */
    node->wgt  = (*tracts)->wgt;/* and initialize the fields */
    node->size = -(node->max = n);
    if (n > 0)                  /* copy the transaction suffix */
      memcpy(node->items, (*tracts)->items +index, n *sizeof(int));
    return node;                /* copy the remaining items and */
  }                             /* return the created leaf node */

  for (w = 0; (--cnt >= 0) && ((*tracts)->size <= index); )
    w += (*tracts++)->wgt;      /* skip t.a. that are too short */
  for (n = 0, item = TA_END, i = ++cnt; --i >= 0; ) {
    w += tracts[i]->wgt;        /* traverse the transactions */
    k  = tracts[i]->items[index];
    if (k != item) { item = k; n++; }
  }                             /* count the different items */
  i = n;                        /* get the size of the item array */
  #ifdef ALIGN8                 /* if to align to multiples of 8 */
  i += (((int)&((TANODE*)0)->items)/sizeof(int) +n) & 1;
  #endif                        /* add an array element if necessary */
  node = (TANODE*)malloc(sizeof(TANODE) +(i-1) *sizeof(int)
                                        + n    *sizeof(TANODE*));
  if (!node) return NULL;       /* create a transaction tree node */
  node->wgt  = w;               /* and initialize its fields */
  node->max  = 0;
  node->size = n;               /* if transactions are captured, */
  if (n <= 0) return node;      /* return the created tree */

  cnds = TAN_CHILDREN(node);    /* get the child pointer array */
  for (--cnt; --n >= 0; cnt = i) {  /* traverse the different items */
    node->items[n] = item = tracts[cnt]->items[index];
    for (i = cnt; --i >= 0; )   /* find trans. with the current item */
      if (tracts[i]->items[index] != item) break;
    cnds[n] = create(tracts +i+1, cnt-i, index+1);
    if (!cnds[n]) break;        /* recursively create a subtree */
    if ((k = cnds[n]->max +1) > node->max) node->max = k;
  }                             /* adapt the maximal remaining size */
  if (n < 0) return node;       /* if successful, return created tree */

  while (++n < node->size) delete(cnds[n]);
  free(node);                   /* on error delete created subtree */
  return NULL;                  /* return 'failure' */
}  /* create() */

/*--------------------------------------------------------------------*/

TATREE* tat_create (TABAG *bag)
{                               /* --- create a transactions tree */
  TATREE *tree;                 /* created transaction tree */

  assert(bag);                  /* check the function argument */
  tree = (TATREE*)malloc(sizeof(TATREE));
  if (!tree) return NULL;       /* create the transaction tree body */
  tree->bag  = bag;             /* note the underlying trans. bag */
  if (bag->cnt <= 0) {          /* if the transaction bag is empty, */
    tree->root = &tree->empty;  /* set an empty root node */
    tree->root->max = tree->root->wgt = tree->root->size = 0; }
  else {                        /* if the bag contains transactions */
    tree->root = create((TRACT**)bag->tracts, bag->cnt, 0);
    if (!tree->root) { free(tree); return NULL; }
  }                             /* recursively build the tree */
  return tree;                  /* return the created trans. tree */
}  /* tat_create() */

/*--------------------------------------------------------------------*/

void tat_delete (TATREE *tree, int del)
{                               /* --- delete a transaction tree */
  assert(tree);                 /* check the function argument */
  delete(tree->root);           /* delete the nodes of the tree */
  if (tree->bag && del) tbg_delete(tree->bag, (del > 1));
  free(tree);                   /* delete the item base and */
}  /* tat_delete() */           /* the transaction tree body */

/*--------------------------------------------------------------------*/

static int nodecnt (const TANODE *root)
{                               /* --- count the nodes */
  int    i, n;                  /* loop variable, number of nodes */
  TANODE **cnds;                /* array of child nodes */

  assert(root);                 /* check the function argument */
  if (root->size <= 0)          /* if this is a leaf node, */
    return 1;                   /* there is only one node */
  cnds = TAN_CHILDREN(root);    /* get the child pointer array */
  for (i = n = 0; i < root->size; i++)
    n += nodecnt(cnds[i]);      /* recursively count the nodes */
  return n+1;                   /* return number of nodes in tree */
}  /* nodecnt() */

/*--------------------------------------------------------------------*/

int tat_size (const TATREE *tree)
{ return nodecnt(tree->root); }

/*--------------------------------------------------------------------*/

int tat_filter (TATREE *tree, int min, const int *marks, int heap)
{                               /* --- filter a transaction tree */
  TABAG *bag;                   /* underlying transaction bag */

  assert(tree);                 /* check the function argument */
  delete(tree->root);           /* delete the nodes of the tree */
  tbg_filter(bag = tree->bag, min, marks, 0);
  tbg_sort  (bag, 0, heap);     /* remove unnec. items and trans. */
  tbg_reduce(bag, 0);           /* and reduce trans. to unique ones */
  tree->root = create((TRACT**)bag->tracts, bag->cnt, 0);
  if (tree->root) return 0;     /* recreate the transaction tree */
  delete(tree->root);           /* on failure delete the nodes */
  tree->root = NULL;            /* and clear the root pointer */
  return -1;                    /* return an error indicator */
}  /* tat_filter() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

static void show (TANODE *node, ITEMBASE *base, int ind)
{                               /* --- rekursive part of tat_show() */
  int    i, k;                  /* loop variables */
  TANODE **cnds;                /* array of child nodes */

  assert(node && (ind >= 0));   /* check the function arguments */
  if (node->size <= 0) {        /* if this is a leaf node */
    for (i = 0; i < node->max; i++)
      printf("%s ", ib_xname(base, node->items[i]));
    printf("[%d]\n", node->wgt);
    return;                     /* print the items in the */
  }                             /* (rest of) the transaction */
  cnds = TAN_CHILDREN(node);    /* get the child pointer array */
  for (i = 0; i < node->size; i++) {
    if (i > 0) for (k = ind; --k >= 0; ) printf("  ");
    printf("%s ", ib_xname(base, node->items[i]));
    show(cnds[i], base, ind+1); /* traverse the items, print them, */
  }                             /* and show the children recursively */
}  /* show() */

/*--------------------------------------------------------------------*/

void tat_show (TATREE *tree)
{                               /* --- show a transaction tree */
  assert(tree);                 /* check the function argument */
  show(tree->root, tbg_base(tree->bag), 0);
}  /* tat_show() */             /* call the recursive function */

#endif
#endif
#endif
/*--------------------------------------------------------------------*/
#ifndef NOMAIN

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (tabag) tbg_delete(tabag, 0); \
  if (tread) trd_delete(tread, 1); \
  if (ibase) ib_delete (ibase);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0, n, w;       /* loop variables, counters */
  char    *s;                   /* to traverse the options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_inp  = NULL;      /* name of input  file */
  CCHAR   *recseps = NULL;      /* record  separators */
  CCHAR   *fldseps = NULL;      /* field   separators */
  CCHAR   *blanks  = NULL;      /* blank   characters */
  CCHAR   *comment = NULL;      /* comment characters */
  double  supp     = -1;        /* minimum support */
  int     sort     = -2;        /* flag for item sorting and recoding */
  int     pack     =  0;        /* flag for packing 16 items */
  int     repeat   =  1;        /* number of repetitions */
  int     mtar     =  0;        /* mode for transaction reading */
  TRACT   **tracts = NULL;      /* array of transactions */
  clock_t t;                    /* timer for measurements */

  #ifndef QUIET                 /* if not quiet version */
  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no arguments given */
    printf("usage: %s [options] infile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-s#      minimum support of an item set           "
                    "(default: %g)\n", supp);
    printf("         (positive: percentage, "
                     "negative: absolute number)\n");
    printf("-q#      sort items w.r.t. their frequency        "
                    "(default: %d)\n", sort);
    printf("         (1: ascending, -1: descending, 0: do not sort,\n"
           "          2: ascending, -2: descending w.r.t. "
                    "transaction size sum)\n");
    printf("-p       pack the 16 items with the lowest codes\n");
    printf("-x#      number of repetitions (for benchmarking) "
                    "(default: 1)\n");
    printf("-w       integer transaction weight in last field "
                    "(default: only items)\n");
    printf("-r#      record/transaction separators            "
                    "(default: \"\\n\")\n");
    printf("-f#      field /item        separators            "
                    "(default: \" \\t,\")\n");
    printf("-b#      blank   characters                       "
                    "(default: \" \\t\\r\")\n");
    printf("-C#      comment characters                       "
                    "(default: \"#\")\n");
    printf("infile   file to read transactions from           "
                    "[required]\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */
  #endif  /* #ifndef QUIET */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (*s) {              /* traverse options */
        switch (*s++) {         /* evaluate switches */
          case 's': supp   =      strtod(s, &s);    break;
          case 'q': sort   = (int)strtol(s, &s, 0); break;
          case 'p': pack   = -1;                    break;
          case 'x': repeat = (int)strtol(s, &s, 0); break;
          case 'w': mtar  |= TA_WEIGHT;             break;
          case 'r': optarg = &recseps;              break;
          case 'f': optarg = &fldseps;              break;
          case 'b': optarg = &blanks;               break;
          case 'C': optarg = &comment;              break;
          default : error(E_OPTION, *--s);          break;
        }                       /* set option variables */
        if (optarg && *s) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-options */
        case  0: fn_inp = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note filenames */
    }
  }
  if (optarg) error(E_OPTARG);  /* check (option) arguments */
  if (k < 1)  error(E_ARGCNT);  /* and number of arguments */
  MSG(stderr, "\n");            /* terminate the startup message */

  /* --- read transaction database --- */
  ibase = ib_create(0, 0);      /* create an item base */
  if (!ibase) error(E_NOMEM);   /* to manage the items */
  tread = trd_create();         /* create a transaction reader */
  if (!tread) error(E_NOMEM);   /* and configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, "", comment);
  tabag = tbg_create(ibase);    /* create a transaction bag */
  if (!tabag) error(E_NOMEM);   /* to store the transactions */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_inp) != 0)
    error(E_FOPEN, trd_name(tread));
  MSG(stderr, "reading %s ... ", trd_name(tread));
  k = tbg_read(tabag, tread, mtar);
  if (k < 0)                    /* read the transaction database */
    error(-k, tbg_errmsg(tabag, NULL, 0));
  trd_delete(tread, 1);         /* close the input file and */
  tread = NULL;                 /* delete the table reader */
  n = ib_cnt(ibase);            /* get the number of items, */
  k = tbg_cnt(tabag);           /* the number of transactions, */
  w = tbg_wgt(tabag);           /* the total transaction weight */
  MSG(stderr, "[%d item(s), %d", n, k);
  if (w != k) MSG(stderr, "/%d", w);
  MSG(stderr, " transaction(s)] done [%.2fs].", SEC_SINCE(t));
  if ((n <= 0) || (k <= 0))     /* check for at least one item */
    error(E_NOITEMS);           /* and at least one transaction */
  MSG(stderr, "\n");            /* compute absolute support value */
  supp = ceil((supp >= 0) ? 0.01 *supp *w : -supp);

  /* --- sort and recode items --- */
  t = clock();                  /* start timer, print log message */
  MSG(stderr, "filtering, sorting and recoding items ... ");
  n = tbg_recode(tabag, (int)supp, -1, -1, sort);
  if (n <  0) error(E_NOMEM);   /* recode items and transactions */
  if (n <= 0) error(E_NOITEMS); /* and check the number of items */
  MSG(stderr, "[%d item(s)] done [%.2fs].\n", n, SEC_SINCE(t));

  /* --- sort and reduce transactions --- */
  t = clock();                  /* start timer, print log message */
  MSG(stderr, "sorting and reducing transactions ... ");
  tbg_filter(tabag, 0,NULL,0);  /* remove items of short transactions */
  tbg_itsort(tabag, +1, 0);     /* sort items in transactions and */
  tracts = (TRACT**)malloc(k *sizeof(TRACT*));
  if (!tracts) error(E_NOMEM);  /* copy transactions to a buffer */
  memcpy(tracts, tabag->tracts, k *sizeof(TRACT*));
  if (pack) tbg_pack(tabag,16); /* pack 16 items with lowest codes */
  for (i = 0; i < repeat; i++){ /* repeated sorting loop */
    memcpy(tabag->tracts, tracts, k *sizeof(TRACT*));
    tbg_sort(tabag, +1, 0);     /* copy back the transactions */
  }                             /* and sort the transactions */
  k = tbg_reduce(tabag, 0);     /* reduce transactions to unique ones */
  free(tracts);                 /* delete the transaction buffer */
  MSG(stderr, "[%d", k); if (w != k) MSG(stderr, "/%d", w);
  MSG(stderr, " transaction(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */

#endif
