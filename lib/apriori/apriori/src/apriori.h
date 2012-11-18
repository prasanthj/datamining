/*----------------------------------------------------------------------
  File    : apriori.h
  Contents: apriori algorithm for finding frequent item sets
  Author  : Christian Borgelt
  History : 2011.07.18 file created
            2011.10.18 several mode flags added
----------------------------------------------------------------------*/
#ifndef __APRIORI__
#define __APRIORI__
#include "istree.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define APR_VERBOSE   INT_MIN   /* verbose message output */
#define APR_TATREE    (IST_PERFECT << 4)  /* use transaction tree */
#define APR_POST      (APR_TATREE  << 1)  /* use a-posteriori pruning */
#ifdef NDEBUG
#define APR_NOCLEAN   (APR_POST    << 1)
#else
#define APR_NOCLEAN   0
#endif

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern int apriori (TABAG *tabag, int target, int mode, int supp,
                    int smax, double conf, int eval, int aggm,
                    double minval, double minimp, int prune,
                    double filter, int dir, ISREPORT *rep);
#endif
