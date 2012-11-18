/*----------------------------------------------------------------------
  File    : istree.h
  Contents: item set tree management for apriori algorithm
  Author  : Christian Borgelt
  History : 1996.01.22 file created as istree.h (renamed 2011.10.12)
            1996.01.29 ISTNODE.offset and ISTNODE.id added
            1996.02.08 several fields added to ISTREE structure
            1996.03.28 support made relative to number of item sets
            1996.11.23 ISTREE.lvls (first nodes of each level) added
            1996.11.24 ISTREE.eval (rule evaluation measure) added
            1997.08.18 chi^2 added, mincnt added to function ist_init()
            1998.02.11 parameter minval added to function ist_init()
            1998.05.14 item set tree navigation functions added
            1998.08.20 structure ISTNODE redesigned
            1998.12.08 function ist_getwgt() added, float -> double
            1999.11.05 rule evaluation measure EM_LDIF added
            1999.11.08 parameter eval added to function ist_rule()
            2001.04.01 functions ist_set() and ist_suppx() added
            2001.12.28 sort function moved to module tract
            2002.02.07 function ist_clear() removed, ist_setwgt() added
            2002.02.11 optional use of identifier maps in nodes added
            2002.02.12 function ist_next() added
            2003.07.17 functions ist_itemcnt() and ist_check() added
            2003.07.18 function ist_mark() added (item set filter)
            2003.08.11 item set filtering generalized (ist_mark())
            2004.05.09 parameter eval added to function ist_set()
            2008.03.24 creation based on ITEMBASE structure
            2008.08.12 adapted to redesign of tract.[hc]
            2008.08.21 function ist_report() added (recursive reporting)
            2008.09.01 parameter smax added to function ist_create()
            2008.09.07 ist_prune() added, memory saving option removed
            2008.09.10 set and rule extraction functions redesigned
            2008.11.19 adapted to modified transaction tree interface
            2009.11.13 evaluation measure flag IST_INVBXS added
            2010.02.04 renamed to istree.h to prevent name clashes
            2010.03.04 several additional evaluation measures added
            2010.08.30 Fisher's exact test added as evaluation measure
            2010.10.22 chi^2 measure with Yates correction added
            2011.08.05 function ist_clomax() added (replaces ist_mark())
            2011.08.16 filter mode ISR_GENERA added for ist_clomax()
----------------------------------------------------------------------*/
#ifndef __ISTREE__
#define __ISTREE__
#include <limits.h>
#ifndef TATREEFN
#define TATREEFN
#endif
#include "ruleval.h"
#include "report.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
/* --- operation modes --- */
#define IST_PERFECT 0x0100      /* prune with perfect extensions */

/* --- additional evaluation measures --- */
/* most definitions in ruleval.h */
#define IST_LDRATIO RE_FNCNT    /* binary log. of support quotient */
#define IST_INVBXS  INT_MIN     /* invalidate eval. below exp. supp. */

/* --- evaluation measure aggregation modes --- */
#define IST_NONE    0           /* no aggregation (use first value) */
#define IST_FIRST   0           /* no aggregation (use first value) */
#define IST_MIN     1           /* minimum of measure values */
#define IST_MAX     2           /* maximum of measure values */
#define IST_AVG     3           /* average of measure values */
#define IST_EQS     4           /* split into equal size subsets */

/* --- item set filter modes --- */
#define IST_CLEAR   ISR_ALL     /* all     frequent item sets */
#define IST_CLOSED  ISR_CLOSED  /* closed  frequent item sets */
#define IST_MAXIMAL ISR_MAXIMAL /* maximal frequent item sets */
#define IST_GENERA  ISR_GENERA  /* generators */
#define IST_SAFE    ISR_SORT    /* safe filtering (assume holes) */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct istnode {        /* --- item set tree node --- */
  struct istnode *succ;         /* successor node (on same level) */
  struct istnode *parent;       /* parent    node (one level up) */
  int            item;          /* item used in parent node */
  int            size;          /* size   of counter array */
  int            offset;        /* offset of counter array */
  int            chcnt;         /* number of child nodes */
  int            cnts[1];       /* counter array (weights) */
} ISTNODE;                      /* (item set tree node) */

typedef struct {                /* --- item set tree --- */
  ITEMBASE *base;               /* underlying item base */
  int      mode;                /* search mode (e.g. support def.) */
  int      wgt;                 /* total weight of transactions */
  int      height;              /* tree height (number of levels) */
  int      maxht;               /* max. height (size of level array) */
  ISTNODE  **lvls;              /* first node of each level */
  int      rule;                /* minimal support of an assoc. rule */
  int      supp;                /* minimal support of an item set */
  int      smax;                /* maximal support of an item set */
  double   conf;                /* minimal confidence of a rule */
  int      eval;                /* additional evaluation measure */
  int      agg;                 /* aggregation mode of measure values */
  int      invbxs;              /* invalidate eval. below expectation */
  int      dir;                 /* direction of evaluation measure */
  double   thresh;              /* evaluation measure threshold */
  double   minimp;              /* minimal improvement of measure */
  ISTNODE  *curr;               /* current node for traversal */
  int      size;                /* current size of an item set */
  int      minsz;               /* minimal size of an item set */
  int      maxsz;               /* maximal size of an item set */
  int      order;               /* item set output order (by size) */
  ISTNODE  *node;               /* item set node for extraction */
  int      index;               /* index in item set node */
  ISTNODE  *head;               /* head item node for extraction */
  int      prune;               /* start level for evaluation pruning */
  int      item;                /* head item of previous rule */
  int      *buf;                /* buffer for paths (support check) */
  int      *path;               /* current path / (partial) item set */
  int      hdonly;              /* head only item in current set */
  int      *map;                /* to create identifier maps */
#ifdef BENCH                    /* if benchmark version */
  int      ndcnt;               /* number of item set tree nodes */
  int      ndprn;               /* number of pruned tree nodes */
  int      mapsz;               /* number of elements in id. maps */
  int      sccnt;               /* number of created support counters */
  int      scnec;               /* number of necessary supp. counters */
  int      scprn;               /* number of pruned support counters */
  int      cpcnt;               /* number of created child pointers */
  int      cpnec;               /* number of necessary child pointers */
  int      cpprn;               /* number of pruned child pointers */
#endif
} ISTREE;                       /* (item set tree) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern ISTREE* ist_create  (ITEMBASE *base, int mode,
                            int supp, int smax, double conf);
extern void    ist_delete  (ISTREE *ist);
extern int     ist_itemcnt (ISTREE *ist);

extern void    ist_count   (ISTREE *ist,
                            const int *items, int n, int wgt);
extern void    ist_countt  (ISTREE *ist, const TRACT  *tract);
extern void    ist_countb  (ISTREE *ist, const TABAG  *bag);
#ifdef TATREEFN
extern void    ist_countx  (ISTREE *ist, const TATREE *tree);
#endif
extern void    ist_commit  (ISTREE *ist);
extern int     ist_check   (ISTREE *ist, int *marks);
extern void    ist_prune   (ISTREE *ist);
extern int     ist_addlvl  (ISTREE *ist);

extern int     ist_height  (ISTREE *ist);
extern int     ist_getwgt  (ISTREE *ist);
extern int     ist_setwgt  (ISTREE *ist, int wgt);
extern int     ist_incwgt  (ISTREE *ist, int wgt);

extern void    ist_up      (ISTREE *ist, int root);
extern int     ist_down    (ISTREE *ist, int item);
extern int     ist_next    (ISTREE *ist, int item);
extern int     ist_supp    (ISTREE *ist, int item);
extern int     ist_suppx   (ISTREE *ist, int *items, int cnt);

extern void    ist_clear   (ISTREE *ist);
extern void    ist_filter  (ISTREE *ist, int size);
extern void    ist_clomax  (ISTREE *ist, int target);
extern void    ist_setsize (ISTREE *ist, int min, int max, int order);
extern void    ist_seteval (ISTREE *ist, int eval, int agg,
                            double thresh, double minimp, int prune);

extern void    ist_init    (ISTREE *ist);
extern int     ist_set     (ISTREE *ist, int *items, int *supp,
                            double *eval);
extern int     ist_rule    (ISTREE *ist, int *rule,  int *supp,
                            int *body, int *head, double *eval);

extern int     ist_report  (ISTREE *ist, ISREPORT *rep);
extern double  ist_eval    (ISTREE *ist);
extern double  ist_evalx   (ISREPORT *rep, void *data);

#ifdef BENCH
extern void    ist_stats   (ISTREE *ist);
#endif
#ifndef NDEBUG
extern void    ist_show    (ISTREE *ist);
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define ist_itemcnt(t)     ((t)->levels[0]->size)
#define ist_height(t)      ((t)->height)
#define ist_getwgt(t)      ((t)->wgt & ~INT_MIN)
#define ist_setwgt(t,n)    ((t)->wgt = (n))
#define ist_incwgt(t,n)    ((t)->wgt = ((t)->wgt & ~INT_MIN) +(n))

#endif
