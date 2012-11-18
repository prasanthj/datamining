/*----------------------------------------------------------------------
  File    : symtab.h
  Contents: symbol table and identifier map management
  Author  : Christian Borgelt
  History : 1995.10.22 file created
            1995.10.30 functions made independent of symbol data
            1995.11.26 symbol types and visibility levels added
            1996.01.04 function st_clear() added (remove all symbols)
            1996.02.27 st_insert() modified, st_name(), st_type() added
            1996.03.26 insertion into hash bin simplified
            1996.06.28 dynamic hash bin array enlargement added
            1997.04.01 functions st_clear() and st_remove() combined
            1998.05.31 list of all symbols removed (inefficient)
            1998.06.20 deletion function moved to st_create()
            1998.09.28 function st_stats() added (for debugging)
            1999.02.04 long int changed to int (assume 32 bit system)
            1999.11.10 special identifier map management added
            2004.12.15 function idm_trunc() added (remove names)
            2008.08.11 function idm_getid() added, changed to CMPFN
            2011.07.12 generalized to arbitrary keys (not just names)
            2011.08.17 size function removed, key size made parameter
----------------------------------------------------------------------*/
#ifndef __SYMTAB__
#define __SYMTAB__
#include <stdio.h>
#include "fntypes.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define EXISTS    ((void*)-1)   /* symbol exists already */
#define IDMAP     SYMTAB        /* id maps are special symbol tables */

/* --- abbreviations for standard function sets --- */
#define ST_STRFN  st_strhash, st_strcmp, NULL
#define ST_INTFN  st_inthash, st_intcmp, NULL

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef unsigned int HASHFN (const void *key, int type);

typedef struct ste {            /* --- symbol table element --- */
  struct ste *succ;             /* successor in hash bin */
  void       *key;              /* symbol name/key */
  int        type;              /* symbol type */
  int        level;             /* visibility level */
} STE;                          /* (symbol table element) */

typedef struct {                /* --- symbol table --- */
  int        cnt;               /* current number of symbols */
  int        level;             /* current visibility level */
  int        size;              /* current hash table size */
  int        max;               /* maximal hash table size */
  HASHFN     *hashfn;           /* hash function */
  CMPFN      *cmpfn;            /* comparison function */
  void       *data;             /* comparison data */
  OBJFN      *delfn;            /* symbol deletion function */
  STE        **bins;            /* array of hash bins */
  int        idsize;            /* size of identifier array */
  int        **ids;             /* identifier array */
} SYMTAB;                       /* (symbol table) */

/*----------------------------------------------------------------------
  Name/Key Functions
----------------------------------------------------------------------*/
extern int          st_strcmp  (const void *a, const void *b, void *d);
extern unsigned int st_strhash (const void *s, int type);

extern int          st_intcmp  (const void *a, const void *b, void *d);
extern unsigned int st_inthash (const void *i, int type);

/*----------------------------------------------------------------------
  Symbol Table Functions
----------------------------------------------------------------------*/
extern SYMTAB*      st_create  (int init, int max, HASHFN hashfn,
                                CMPFN cmpfn, void *data, OBJFN delfn);
extern void         st_delete  (SYMTAB *tab);
extern void*        st_insert  (SYMTAB *tab, const void *key, int type,
                                size_t keysize, size_t datasize);
extern int          st_remove  (SYMTAB *tab, const void *key, int type);
extern void*        st_lookup  (SYMTAB *tab, const void *key, int type);
extern void         st_begblk  (SYMTAB *tab);
extern void         st_endblk  (SYMTAB *tab);
extern int          st_symcnt  (const SYMTAB *tab);
extern const char*  st_name    (const void *data);
extern const void*  st_key     (const void *data);
extern int          st_type    (const void *data);
#ifndef NDEBUG
extern void         st_stats   (const SYMTAB *tab);
#endif

/*----------------------------------------------------------------------
  Name/Identifier Map Functions
----------------------------------------------------------------------*/
#ifdef IDMAPFN
extern IDMAP*       idm_create (int init, int max, HASHFN hashfn,
                                CMPFN cmpfn, void *data, OBJFN delfn);
extern void         idm_delete (IDMAP* idm);
extern void*        idm_add    (IDMAP* idm, const void *key,
                                size_t keysize, size_t datasize);
extern void*        idm_byname (IDMAP* idm, const char *name);
extern void*        idm_bykey  (IDMAP* idm, const void *key);
extern void*        idm_byid   (IDMAP* idm, int id);
extern int          idm_getid  (IDMAP* idm, const char *name);
extern const char*  idm_name   (const void *data);
extern const void*  idm_key    (const void *data);
extern int          idm_cnt    (const IDMAP *idm);
extern void         idm_sort   (IDMAP *idm, CMPFN cmpfn, void *data,
                                int *map, int dir);
extern void         idm_trunc  (IDMAP *idm, int n);
#ifndef NDEBUG
extern void         idm_stats  (const IDMAP *idm);
#endif
#endif
/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define st_begblk(t)      ((t)->level++)
#define st_symcnt(t)      ((t)->cnt)
#define st_name(d)        ((const char*)((STE*)(d)-1)->key)
#define st_key(d)         ((const void*)((STE*)(d)-1)->key)
#define st_type(d)        (((STE*)(d)-1)->type)

/*--------------------------------------------------------------------*/
#ifdef IDMAPFN
#define idm_delete(m)     st_delete(m)
#define idm_add(m,n,k,s)  st_insert(m,n,0,k,s)
#define idm_byname(m,n)   st_lookup(m,n,0)
#define idm_bykey(m,k)    st_lookup(m,k,0)
#define idm_byid(m,i)     ((void*)(m)->ids[i])
#define idm_name(d)       st_name(d)
#define idm_key(d)        st_key(d)
#define idm_cnt(m)        st_symcnt(m)
#ifndef NDEBUG
#define idm_stats(m)      st_stats(m)
#endif
#endif
#endif
