/*----------------------------------------------------------------------
  File    : symtab.c
  Contents: symbol table management
  Author  : Christian Borgelt
  History : 1995.10.22 file created
            1995.10.30 functions made independent of symbol data
            1995.11.26 symbol types and visibility levels added
            1996.01.04 function st_clear() added (remove all symbols)
            1996.02.27 function st_insert() modified
            1996.06.28 dynamic hash bin array enlargement added
            1996.07.04 bug in hash bin reorganization removed
            1997.04.01 functions st_clear() and st_remove() combined
            1998.02.06 default table sizes changed (enlarged)
            1998.05.31 list of all symbols removed (inefficient)
            1998.06.20 deletion function moved to st_create()
            1998.09.01 bug in function sort() fixed, assertions added
            1998.09.25 hash function improved (shifts and exclusive ors)
            1998.09.28 types ULONG and CCHAR removed, st_stats() added
            1999.02.04 long int changed to int (32 bit systems)
            1999.11.10 name/identifier map management added
            2003.08.15 renamed new to nel in st_insert() (C++ compat.)
            2004.12.15 function idm_trunc() added (truncate lists)
            2004.12.28 bug in function idm_trunc() fixed
            2008.08.11 function idm_getid() added (get id for name)
            2010.08.20 symbol table reorganization redesigned
            2011.07.08 string hash function improved and simplified
            2011.07.12 generalized to arbitrary keys (not just names)
            2011.08.16 default initial size increased to 65535 bins
            2012.07.03 another hash function added (different factor)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "symtab.h"
#ifdef IDMAPFN
#include "arrays.h"
#endif
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define DFLT_INIT    65535      /* default initial hash table size */
#define DFLT_MAX   4194303      /* default maximal hash table size */
#define BLKSIZE       4096      /* block size for identifier array */

#ifdef ALIGN8
#define ALIGN            8      /* alignment to addresses that are */
#else                           /* divisible by 8 (64 bit) */
#define ALIGN            4      /* alignment to addresses that are */
#endif                          /* divisible by 8 (32 bit) */

/*----------------------------------------------------------------------
  Name/Key Functions
----------------------------------------------------------------------*/

int st_strcmp (const void *a, const void *b, void *data)
{                               /* --- string comparison function */
  return strcmp((const char*)a, (const char*)b);
}  /* st_strcmp() */

/*--------------------------------------------------------------------*/

unsigned int st_strhash (const void *s, int type)
{                               /* --- string hash function */
  register const char  *p = (const char*)s;  /* to traverse the key */
  register unsigned int h = type;            /* hash value */
  /* Java: */
  /* while (*p) h = h *31 +(unsigned int)(*p++); */
  /* djb2: */
  /* h += 5381; */
  /* while (*p) h = h *33 +(unsigned int)(*p++); */
  /* sdbm: */
  /* while (*p) h = h *65599 +(unsigned int)(*p++); */
  /* own designs: */
  /* while (*p) h = h *61 +(unsigned int)(*p++); */
  while (*p) h = h *251 +(unsigned int)(*p++);
  /* while (*p) h = h *16777619 +(unsigned int)(*p++); */
  return h;                     /* compute and return hash value */
}  /* st_strhash() */

/*--------------------------------------------------------------------*/

size_t st_strsize (const void *s)
{                               /* --- string size function */
  return strlen((const char*)s) +1;
}  /* st_strsize() */

/*--------------------------------------------------------------------*/

int st_intcmp (const void *a, const void *b, void *data)
{                               /* --- integer comparison function */
  if (*(const int*)a < *(const int*)b) return -1;
  if (*(const int*)a > *(const int*)b) return +1;
  return 0;                     /* return sign of difference */
}  /* st_intcmp() */

/*--------------------------------------------------------------------*/

unsigned int st_inthash (const void *i, int type)
{                               /* --- integer hash function */
  return (unsigned int)(*(const int*)i ^ type);
}  /* st_inthash() */

/*--------------------------------------------------------------------*/

size_t st_intsize (const void *i)
{ return sizeof(int); }         /* --- integer size function */

/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/

static void delsym (SYMTAB *tab)
{                               /* --- delete all symbols */
  int i;                        /* loop variable */
  STE *e, *t;                   /* to traverse the symbol list */

  assert(tab);                  /* check the function argument */
  for (i = tab->size; --i >= 0; ) {
    e = tab->bins[i];           /* traverse the bin array */
    tab->bins[i] = NULL;        /* clear the current bin */
    while (e) {                 /* traverse the bin list */
      t = e; e = e->succ;       /* note the symbol and get next */
      if (tab->delfn) tab->delfn(t+1);
      free(t);                  /* if a deletion function is given, */
    }                           /* call it and then deallocate */
  }                             /* the symbol table element */
}  /* delsym() */

/*--------------------------------------------------------------------*/

static STE* sort (STE *list)
{                               /* --- sort a transaction list */
  STE *a, *b = list;            /* to traverse the lists */
  STE **e;                      /* end of the output list */

  for (a = list->succ; a; ) {   /* traverse the list to sort */
    a = a->succ;                /* two steps on a, one step on list */
    if (a) { a = a->succ; list = list->succ; }
  }                             /* (split list into two halves) */
  a = list->succ;               /* get the second list and */
  list->succ = NULL;            /* terminate the first list */
  if (a->succ) a = sort(a);     /* and sort them recursively */
  if (b->succ) b = sort(b);     /* if longer than one element */
  for (e = &list; 1; ) {        /* transaction list merge loop */
    if (a->level < b->level) {  /* copy element from first  list */
      *e = a; e = &a->succ; if (!(a = *e)) break; }
    else {                      /* copy element from second list */
      *e = b; e = &b->succ; if (!(b = *e)) break; }
  }                             /* (sort by visibility level) */
  *e = (a) ? a : b;             /* append the non-empty list */
  return list;                  /* return the sorted list */
}  /* sort() */

/*--------------------------------------------------------------------*/

static void rehash (SYMTAB *tab)
{                               /* --- reorganize a hash table */
  int i, k, size;               /* loop variables, new bin array size */
  STE **p, *e, *t;              /* to traverse symbol table elements */

  assert(tab);                  /* check the function argument */
  size = (tab->size << 1) +1;   /* calculate new bin array size */
  if ((size > tab->max)         /* if new size exceeds maximum */
  && ((size = tab->max) <= tab->size))
    return;                     /* set the maximal size */
  p = (STE**)calloc(size, sizeof(STE*));
  if (!p) return;               /* get an enlarged bin array */
  for (i = tab->size; --i >= 0; ) {
    for (e = tab->bins[i]; e; ) {
      t = e; e = e->succ;       /* traverse the old hash bins */
      k = tab->hashfn(t->key, t->type) % size;
      t->succ = p[k]; p[k] = t; /* compute the hash bin index */
    }                           /* and insert the symbol at */
  }                             /* the head of the bin list */
  free(tab->bins);              /* delete  the old bin array */
  tab->bins = p;                /* and set the new bin array */
  for (i = tab->size = size; --i >= 0; )
    if (p[i] && p[i]->succ)     /* sort all bin lists according */
      p[i] = sort(p[i]);        /* to the visibility level */
}  /* rehash() */

/*----------------------------------------------------------------------
  Symbol Table Functions
----------------------------------------------------------------------*/

SYMTAB* st_create (int init, int max, HASHFN hashfn,
                   CMPFN cmpfn, void *data, OBJFN delfn)
{                               /* --- create a symbol table */
  SYMTAB *tab;                  /* created symbol table */

  if (init <= 0) init = DFLT_INIT;  /* check and adapt the initial */
  if (max  <= 0) max  = DFLT_MAX;   /* and maximal bin array size */
  tab = (SYMTAB*)malloc(sizeof(SYMTAB));
  if (!tab) return NULL;        /* allocate symbol table body */
  tab->bins = (STE**)calloc(init, sizeof(STE*));
  if (!tab->bins) { free(tab); return NULL; }
  tab->level  = tab->cnt = 0;   /* allocate the hash bin array */
  tab->size   = init;           /* and initialize fields */
  tab->max    = max;            /* of symbol table body */
  tab->hashfn = (hashfn) ? hashfn : st_strhash;
  tab->cmpfn  = (cmpfn)  ? cmpfn  : st_strcmp;
  tab->data   = data;
  tab->delfn  = delfn;
  tab->idsize = INT_MAX;
  tab->ids    = NULL;
  return tab;                   /* return created symbol table */
}  /* st_create() */

/*--------------------------------------------------------------------*/

void st_delete (SYMTAB *tab)
{                               /* --- delete a symbol table */
  assert(tab && tab->bins);     /* check argument */
  delsym(tab);                  /* delete all symbols, */
  free(tab->bins);              /* the hash bin array, */
  if (tab->ids) free(tab->ids); /* the identifier array, */
  free(tab);                    /* and the symbol table body */
}  /* st_delete() */

/*--------------------------------------------------------------------*/

void* st_insert (SYMTAB *tab, const void *key, int type,
                 size_t keysize, size_t datasize)
{                               /* --- insert a symbol (name/key) */
  unsigned int h;               /* hash value */
  int          i;               /* index of hash bin, buffer */
  STE          *e, *n;          /* to traverse a bin list */

  assert(tab && key             /* check the function arguments */
  &&    ((datasize >= sizeof(int)) || (tab->idsize == INT_MAX)));
  if ((tab->cnt  > tab->size)   /* if the bins are rather full and */
  &&  (tab->size < tab->max))   /* table does not have maximal size, */
    rehash(tab);                /* reorganize the hash table */

  h = tab->hashfn(key, type);   /* compute the hash value and */
  i = h % tab->size;            /* the index of the hash bin */
  for (e = tab->bins[i]; e; e = e->succ)
    if ((type == e->type) && (tab->cmpfn(key, e->key, tab->data) == 0))
      break;                    /* check whether symbol exists */
  if (e && (e->level == tab->level))
    return EXISTS;              /* if symbol found on current level */

  #ifdef IDMAPFN                /* if key/identifier map management */
  if (tab->cnt >= tab->idsize){ /* if the identifier array is full */
    int **p, s = tab->idsize;   /* (new) id array and its size */
    s += (s > BLKSIZE) ? s >> 1 : BLKSIZE;
    p  = (int**)realloc(tab->ids, s *sizeof(int*));
    if (!p) return NULL;        /* resize the identifier array and */
    tab->ids = p; tab->idsize = s;   /* set new array and its size */
  }                             /* (no resizing for symbol tables */
  #endif                        /* since then tab->idsize = MAX_INT) */
  datasize = ((datasize +ALIGN-1) /ALIGN) *ALIGN;
  n = (STE*)malloc(sizeof(STE) +datasize +keysize);
  if (!n) return NULL;          /* allocate memory for new symbol */
  memcpy(n->key = (char*)(n+1) +datasize, key, keysize);
  n->type  = type;              /* note the symbol name/key, type, */
  n->level = tab->level;        /* and the current visibility level */
  n->succ  = tab->bins[i];      /* insert new symbol at the head */
  tab->bins[i] = n++;           /* of the hash bin list */
  #ifdef IDMAPFN                /* if key/identifier maps are */
  if (tab->ids) {               /* supported and this is such a map */
    tab->ids[tab->cnt] = (int*)n;
    *(int*)n = tab->cnt;        /* store the new symbol */
  }                             /* in the identifier array */
  #endif                        /* and set the symbol identifier */
  tab->cnt++;                   /* increment the symbol counter */
  return n;                     /* return pointer to data field */
}  /* st_insert() */

/*--------------------------------------------------------------------*/

int st_remove (SYMTAB *tab, const void *key, int type)
{                               /* --- remove a symbol/all symbols */
  int i;                        /* index of hash bin */
  STE **p, *e;                  /* to traverse a hash bin list */

  assert(tab);                  /* check the function arguments */
  if (!key) {                   /* if no symbol name/key given */
    delsym(tab);                /* delete all symbols */
    tab->cnt = tab->level = 0;  /* reset visibility level */
    return 0;                   /* and symbol counter */
  }                             /* and return 'ok' */
  i = tab->hashfn(key, type) % tab->size;
  p = tab->bins +i;             /* compute index of hash bin */
  while (*p) {                  /* and traverse the bin list */
    if (((*p)->type == type)    /* if the symbol was found */
    &&  (tab->cmpfn(key, (*p)->key, tab->data) == 0))
      break;                    /* abort the search loop */
    p = &(*p)->succ;            /* otherwise get the successor */
  }                             /* in the hash bin */
  e = *p;                       /* if the symbol does not exist, */
  if (!e) return -1;            /* abort the function with failure */
  *p = e->succ;                 /* remove symbol from hash bin */
  if (tab->delfn) tab->delfn(e+1);      /* delete user data */
  free(e);                      /* and symbol table element */
  tab->cnt--;                   /* decrement symbol counter */
  return 0;                     /* return 'ok' */
}  /* st_remove() */

/*--------------------------------------------------------------------*/

void* st_lookup (SYMTAB *tab, const void *key, int type)
{                               /* --- look up a symbol */
  int i;                        /* index of hash bin */
  STE *e;                       /* to traverse a hash bin list */

  assert(tab && key);           /* check the function arguments */
  i = tab->hashfn(key, type) % tab->size;
  e = tab->bins[i];             /* compute index of hash bin */
  while (e) {                   /* and traverse the bin list */
    if ((e->type == type) && (tab->cmpfn(key, e->key, tab->data) == 0))
      return e +1;              /* if symbol found, return its data */
    e = e->succ;                /* otherwise get the successor */
  }                             /* in the hash bin */
  return NULL;                  /* return 'not found' */
}  /* st_lookup() */

/*--------------------------------------------------------------------*/

void st_endblk (SYMTAB *tab)
{                               /* --- remove one visibility level */
  int i;                        /* loop variable */
  STE *e, *t;                   /* to traverse bin lists */

  assert(tab);                  /* check for a valid symbol table */
  if (tab->level <= 0) return;  /* if on level 0, abort */
  for (i = tab->size; --i >= 0; ) {  /* traverse the bin array */
    e = tab->bins[i];           /* remove all symbols of higher level */
    while (e && (e->level >= tab->level)) {
      t = e; e = e->succ;       /* note symbol and get successor */
      if (tab->delfn) tab->delfn(t+1);
      free(t);                  /* delete user data and */
      tab->cnt--;               /* symbol table element and */
    }                           /* decrement symbol counter */
    tab->bins[i] = e;           /* set new start of bin list */
  }
  tab->level--;                 /* go up one level */
}  /* st_endblk() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void st_stats (const SYMTAB *tab)
{                               /* --- compute and print statistics */
  const STE *e;                 /* to traverse hash bin lists */
  int i;                        /* loop variable */
  int used;                     /* number of used hash bins */
  int len;                      /* length of current bin list */
  int min, max;                 /* min. and max. bin list length */
  int cnts[10];                 /* counter for bin list lengths */

  assert(tab);                  /* check for a valid symbol table */
  min = INT_MAX; max = used = 0;/* initialize variables */
  for (i = 10; --i >= 0; ) cnts[i] = 0;
  for (i = tab->size; --i >= 0; ) { /* traverse the bin array */
    len = 0;                    /* determine bin list length */
    for (e = tab->bins[i]; e; e = e->succ) len++;
    if (len > 0) used++;        /* count used hash bins */
    if (len < min) min = len;   /* determine minimal and */
    if (len > max) max = len;   /* maximal list length */
    cnts[(len >= 9) ? 9 : len]++;
  }                             /* count list length */
  printf("number of symbols  : %d\n", tab->cnt);
  printf("number of hash bins: %d\n", tab->size);
  printf("used hash bins     : %d\n", used);
  printf("minimal list length: %d\n", min);
  printf("maximal list length: %d\n", max);
  printf("average list length: %g\n", (double)tab->cnt/tab->size);
  printf("ditto, of used bins: %g\n", (double)tab->cnt/used);
  printf("length distribution:\n");
  for (i = 0; i < 9; i++) printf("%6d ", i);
  printf("    >8\n");
  for (i = 0; i < 9; i++) printf("%6d ", cnts[i]);
  printf("%6d\n", cnts[9]);
}  /* st_stats() */

#endif
/*----------------------------------------------------------------------
  Name/Identifier Map Functions
----------------------------------------------------------------------*/
#ifdef IDMAPFN

IDMAP* idm_create (int init, int max, HASHFN hashfn,
                   CMPFN cmpfn, void *data, OBJFN delfn)
{                               /* --- create a name/identifier map */
  IDMAP *idm;                   /* created name/identifier map */

  idm = st_create(init, max, hashfn, cmpfn, data, delfn);
  if (!idm) return NULL;        /* create a name/identifier map */
  idm->idsize = 0;              /* and clear the id. array size */
  return idm;                   /* return created name/id map */
}  /* idm_create() */

/*--------------------------------------------------------------------*/

int idm_getid (IDMAP *idm, const char *name)
{                               /* --- get an item identifier */
  STE *p = (STE*)idm_byname(idm, name);
  return (p) ? *(int*)p : -1;   /* look up the given name and */
}  /* idm_getid() */            /* return its identifier or -1 */

/*--------------------------------------------------------------------*/

void idm_sort (IDMAP *idm, CMPFN cmpfn, void *data, int *map, int dir)
{                               /* --- sort name/identifier map */
  int i;                        /* loop variable */
  int **p;                      /* to traverse the value array */

  assert(idm && cmpfn);         /* check the function arguments */
  ptr_qsort(idm->ids, idm->cnt, cmpfn, data);
  if (!map) {                   /* if no conversion map is requested */
    for (p = idm->ids +(i = idm->cnt); --i >= 0; )
      **--p = i; }              /* just set new identifiers */
  else {                        /* if a conversion map is requested, */
    p = idm->ids +(i = idm->cnt);       /* traverse the sorted array */
    if (dir < 0)                /* if backward map (i.e. new -> old) */
      while (--i >= 0) { map[i] = **--p; **p = i; }
    else                        /* if forward  map (i.e. old -> new) */
      while (--i >= 0) { map[**--p] = i; **p = i; }
  }                             /* (build conversion map) */
}  /* idm_sort() */

/*--------------------------------------------------------------------*/

void idm_trunc (IDMAP *idm, int n)
{                               /* --- truncate name/identifier map */
  int *id;                      /* to access the identifiers */

  while (idm->cnt > n) {        /* while to remove mappings */
    id = idm->ids[idm->cnt -1]; /* get the identifier object */
    st_remove(idm, st_name(id), 0);
  }                             /* remove the symbol table element */
}  /* idm_trunc() */

#endif
