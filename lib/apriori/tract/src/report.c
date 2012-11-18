/*----------------------------------------------------------------------
  File    : report.c
  Contents: item set reporter management
  Author  : Christian Borgelt
  History : 2008.08.18 item set reporter created in tract.[ch]
            2008.08.30 handling of perfect extensions completed
            2008.09.01 handling of closed and maximal item sets added
            2008.09.08 functions isr_intout() and isr_numout() added
            2008.10.30 transaction identifier reporting added
            2008.10.31 item set reporter made a separate module
            2008.11.01 optional double precision support added
            2008.12.05 bug handling real-valued support fixed (_report)
            2009.10.15 counting of reported item sets added
            2010.02.11 closed/maximal item set filtering added
            2010.02.12 bugs in prefix tree handling fixed (clomax)
            2010.03.09 bug in reporting the empty item set fixed
            2010.03.11 filtering of maximal item sets improved
            2010.03.17 head union tail pruning for maximal sets added
            2010.03.18 parallel item set support and weight reporting
            2010.04.07 extended information reporting functions removed
            2010.07.01 correct output of infinite float values added
            2010.07.02 order of closed/maximal and size filtering fixed
            2010.07.04 bug in isr_report() fixed (closed set filtering)
            2010.07.12 null output file made possible (for benchmarking)
            2010.07.19 bug in function isr_report() fixed (clomax)
            2010.07.21 early closed/maximal repository pruning added
            2010.07.22 adapted to closed/maximal item set filter
            2010.08.06 function isr_direct() for direct reporting added
            2010.08.11 function isr_directx() for extended items added
            2010.08.14 item set header for output added to isr_create()
            2010.10.15 functions isr_open(), isr_close(), isr_rule()
            2010.10.27 handling of null names in isr_open() changed
            2011.05.06 generalized to support type SUPP_T (int/double)
            2011.06.10 function isr_wgtsupp() added (weight/support)
            2011.07.12 adapted to optional integer item names
            2011.07.23 parameter dir added to function isr_seteval()
            2011.08.16 filtering for generators added (with hash table)
            2011.08.17 header/separator/implication sign copied
            2011.08.19 item sorting for generator filtering added
            2011.08.27 no explicit item set generation for no output
            2011.08.29 internal file write buffer added (faster output)
            2011.09.20 internal repository for filtering made optional
            2011.09.27 bug in function isr_report() fixed (item counter)
            2011.10.18 bug in function fastchk() fixed (hdr/sep check)
            2011.10.21 output of floating point numbers improved
            2012.04.10 function isr_addnc() added (no perf. ext. check)
            2012.04.17 weights and logarithms initialized to zero
            2012.05.30 function isr_addpexpk() added (packed items)
            2012.07.23 format character 'd' added (absolute support)
            2012.10.16 bug in function isr_rinfo() fixed ("L", lift)
            2012.10.26 bug in function fastout() fixed (empty set)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <assert.h>
#include <math.h>
#include "report.h"
#include "scanner.h"
#ifdef STORAGE
#include "storage.h"
#endif

#ifdef _MSC_VER
#define isnan(x)      _isnan(x)
#define isinf(x)    (!_isnan(x) && !_finite(x))
#endif                          /* MSC still does not support C99 */

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define BS_WRITE    65536       /* size of internal write buffer */
#define BS_INT         32       /* buffer size for integer output */
#define BS_FLOAT       80       /* buffer size for float   output */
#define LN_2        0.69314718055994530942  /* ln(2) */
#define MODEMASK    (ISR_TARGET|ISR_NOEXPAND|ISR_SORT)

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const double pows[] = {  /* several powers of ten */
  1e-02, 1e-01,                 /* for floating point number output */
  1e+00, 1e+01, 1e+02, 1e+03, 1e+04, 1e+05, 1e+06, 1e+07,
  1e+08, 1e+09, 1e+10, 1e+11, 1e+12, 1e+13, 1e+14, 1e+15,
  1e+16, 1e+17, 1e+18, 1e+19, 1e+20, 1e+21, 1e+22, 1e+23,
  1e+24, 1e+25, 1e+26, 1e+27, 1e+28, 1e+29, 1e+30, 1e+31,
  1e+32, 1e+33 };

/*----------------------------------------------------------------------
  Basic Output Functions
----------------------------------------------------------------------*/

static void fastchk (ISREPORT *rep)
{                               /* --- check for fast output mode */
  if (rep->repofn               /* if there is a report function */
  ||  rep->evalfn               /* or an evaluation function */
  ||  rep->tidfile)             /* or trans ids. are to be written, */
    rep->fast =  0;             /* standard output has to be used */
  else if (!rep->file)          /* if no output (and no filtering), */
    rep->fast = -1;             /* only count the item sets */
  else {                        /* if only an output file is written */
    rep->fast = ((rep->min <= 1) && (rep->max >= INT_MAX)
              && ((strcmp(rep->format, " (%a)") == 0)
              ||  (strcmp(rep->format, " (%d)") == 0))
              &&  (strcmp(rep->hdr,    "")      == 0)
              &&  (strcmp(rep->sep,    " ")     == 0)) ? +1 : 0;
  }                             /* check standard reporting settings */
}  /* fastchk() */

/*--------------------------------------------------------------------*/

static int getsd (const char *s, const char **end)
{                               /* --- get number of signif. digits */
  int k = 6;                    /* number of significant digits */

  assert(s && end);             /* check the function arguments */
  if ((*s >= '0') && (*s <= '9')) {
    k = *s++ -'0';              /* get the first digit */
    if ((*s >= '0') && (*s <= '9'))
      k = 10 *k +*s++ -'0';     /* get a possible second digit and */
  }                             /* compute the number of digits */
  *end = s; return k;           /* return  the number of digits */
}  /* getsd() */

/*--------------------------------------------------------------------*/

static void isr_flush (ISREPORT *rep)
{                               /* --- flush the output buffer */
  assert(rep);                  /* check the function arguments */
  fwrite(rep->buf, sizeof(char), rep->next -rep->buf, rep->file);
  rep->next = rep->buf;         /* write the output buffer */
}  /* isr_flush() */

/*--------------------------------------------------------------------*/

static void isr_putc (ISREPORT *rep, int c)
{                               /* --- write a single character */
  assert(rep);                  /* check the function arguments */
  if (rep->next >= rep->end)    /* if the output buffer is full, */
    isr_flush(rep);             /* flush it (write it to the file) */
  *rep->next++ = (char)c;       /* store the given character */
}  /* isr_putc() */

/*--------------------------------------------------------------------*/

static void isr_puts (ISREPORT *rep, const char *s)
{                               /* --- write a character string */
  assert(rep);                  /* check the function arguments */
  while (*s) {                  /* while not at end of string */
    if (rep->next >= rep->end)  /* if the output buffer is full, */
      isr_flush(rep);           /* flush it (write it to the file) */
    *rep->next++ = *s++;        /* store the next string character */
  }
}  /* isr_puts() */

/*--------------------------------------------------------------------*/

static void isr_putsn (ISREPORT *rep, const char *s, int n)
{                               /* --- write a character string */
  int k;                        /* number of chars in buffer */

  assert(rep);                  /* check the function arguments */
  while (n > 0) {               /* while there are characters left */
    k = rep->end -rep->next;    /* get free space in write buffer */
    if (k >= n){                /* if the string fits into buffer */
      memcpy(rep->next, s, n *sizeof(char));
      rep->next += n; break;    /* simply copy the string into */
    }                           /* the write buffer and abort */
    memcpy(rep->next, s, k *sizeof(char));
    s += k; n -= k; rep->next = rep->end;
    isr_flush(rep);             /* fill the buffer, then flush it, */
  }                             /* and reduce the remaining string */
}  /* isr_putsn() */

/*--------------------------------------------------------------------*/

int isr_intout (ISREPORT *rep, int num)
{                               /* --- print an integer number */
  int  i = BS_INT, n;           /* loop variable, character counter */
  char buf[BS_INT];             /* output buffer */

  assert(rep);                  /* check the function arguments */
  if (num == 0) {               /* treat zero as a special case */
    isr_putc(rep, '0'); return 1; }
  if (num <= INT_MIN) {         /* treat INT_MIN as a special case */
    isr_puts(rep, "-2147483648"); return 11; }
  n = 0;                        /* default: no sign printed */
  if (num < 0) {                /* if the number is negative, */
    num = -num; isr_putc(rep, '-'); n = 1; }  /* print a sign */
  do {                          /* digit output loop */
    buf[--i] = (num % 10) +'0'; /* store the next digit and */
    num /= 10;                  /* remove it from the number */
  } while (num > 0);            /* while there are more digits */
  isr_putsn(rep, buf+i, BS_INT-i);
  n += BS_INT-i;                /* print the generated digits and */
  return n;                     /* return the number of characters */
}  /* isr_intout() */

/*--------------------------------------------------------------------*/

int mantout (ISREPORT *rep, double num, int digits, int ints)
{                               /* --- format a non-negative mantissa */
  int    i, n;                  /* loop variables, sign flag */
  double x, y;                  /* integral and fractional part */
  char   *s, *e, *d;            /* pointers into the output buffer */
  char   buf[BS_FLOAT];         /* output buffer */

  assert(rep);                  /* check the function arguments */
  i = abs(dbl_bsearch(num, pows, 36) +1);
  n = digits -(i-2);            /* compute the number of decimals */
  x = floor(num); y = num-x;    /* split into integer and fraction */
  e = d = buf +40;              /* get buffer for the decimals */
  if (n > 0) {                  /* if to print decimal digits, */
    *e++ = '.';                 /* store a decimal point */
    do { y *= 10;               /* compute the next decimal */
      *e++ = (int)y +'0';       /* and store it in the buffer */
      y   -= floor(y);          /* remove the printed decimal */
    } while (--n > 0);          /* while there are more decimals */
  }                             /* remove a decimal if necessary */
  if ((y > 0.5) || ((y == 0.5)  /* if number needs to be rounded */
  &&  ((e > d) ? *(e-1) & 1 : floor(x/2) >= x/2))) {
    for (s = e; --s > d; ) {    /* traverse the decimal digits */
      if (*s < '9') { (*s)++; break; }
      *s = '0';                 /* if digit can be incremented, */
    }                           /* abort, otherwise store a zero */
    if ((s <= d) && ((x += 1) >= pows[i]))
      if (--e <= d+1) e = d;    /* if all decimals have been adapted, */
  }                             /* increment the integer part and */
  if (e > d) {                  /* if there are decimal places, */
    while (*--e == '0');        /* remove all trailing zeros */
    if (e > d) e++;             /* if there are no decimals left, */
  }                             /* also remove the decimal point */
  s = d;                        /* adapt the decimals if necessary */
  do {                          /* integral part output loop */
    *--s = (char)fmod(x, 10) +'0';
    x = floor(x/10);            /* compute and store next digit */
  } while (x > 0);              /* while there are more digits */
  if ((n = d-s) > ints)         /* check size of integral part */
    return -n;                  /* and abort if it is too large */
  isr_putsn(rep, s, n = e-s);   /* print the formatted number */
  return n;                     /* return the number of characters */
}  /* mantout() */

/*--------------------------------------------------------------------*/

int isr_numout (ISREPORT *rep, double num, int digits)
{                               /* --- print a floating point number */
  int  k, n, e;                 /* character counters and exponent */
  char buf[BS_FLOAT];           /* output buffer */

  assert(rep);                  /* check the function arguments */
  if (isnan(num)) {             /* check for 'not a number' */
    isr_puts(rep, "nan"); return 3; }
  n = 0;                        /* default: no sign printed */
  if (num < 0) {                /* if the number is negative, */
    num = -num; isr_putc(rep, '-'); n = 1; }  /* print a sign */
  if (isinf(num)) {             /* check for an infinite value */
    isr_puts(rep, "inf"); return n+3; }
  if (num < DBL_MIN) {          /* check for a zero value */
    isr_putc(rep, '0');   return n+1; }
  if (digits > 32) digits = 32; /* limit the number of sign. digits */
  if (digits > 11) {            /* if very high precision is needed */
    k = sprintf(buf, "%.*g", digits, num);
    isr_putsn(rep, buf, k);     /* format with standard printf, */
    return n+k;                 /* print the formatted number and */
  }                             /* return the number of characters */
  e = 0;                        /* default: no exponential represent. */
  if ((num >= pows[digits+2])   /* if an exponential representation */
  ||  (num <  0.001)) {         /* is of the number is preferable */
    while (num <  1e00) { num *= 1e32; e -= 32; }
    while (num >= 1e32) { num /= 1e32; e += 32; }
    k = dbl_bsearch(num, pows+2, 34);
    if (k < 0) k = -2-k;        /* find the decimal exponent and */
    e += k;                     /* extract it from the number */
    num /= pows[k+2];           /* compute the new mantissa */
  }                             /* (one digit before decimal point) */
  k = mantout(rep, num, digits, (e == 0) ? digits : 1);
  if (k < 0) {                  /* try to output the mantissa */
    num /= pows[1-k]; e += -1-k;/* on failure adapt the mantissa */
    k = mantout(rep, num, digits, 1);
  }                             /* output the adapted number */
  n += k;                       /* compute number of printed chars. */
  if (e == 0) return n;         /* if no exponent, abort the function */
  isr_putc(rep, 'e'); n += 2;   /* print an exponent indicator */
  isr_putc(rep, (e < 0) ? '-' : '+');
  if ((e = abs(e)) < 10) { isr_putc(rep, '0'); n++; }
  k = BS_INT;                   /* get the end of the buffer */
  do {                          /* exponent digit output loop */
    buf[--k] = (e % 10) +'0';   /* store the next digit and */
    e /= 10;                    /* remove it from the number */
  } while (e > 0);              /* while there are more digits */
  isr_putsn(rep, buf+k, BS_INT-k);
  return n+BS_INT-k;            /* print the generated digits and */
}  /* isr_numout() */           /* return the number of characters */

/* It is (significantly) faster to output a floating point number  */
/* with the above routines than with sprintf. However, the above   */
/* code produces slightly less accurate output for more than about */
/* 14 significant digits. For those cases sprintf is used instead. */

/*--------------------------------------------------------------------*/

int isr_wgtout (ISREPORT *rep, SUPP_T supp, double wgt)
{                               /* --- print an item weight */
  int        k, n = 0;          /* number of decimals, char. counter */
  const char *s, *t;            /* to traverse the format */

  assert(rep);                  /* check the function arguments */
  if (!rep->iwfmt || !rep->file)
    return 0;                   /* check for a given format and file */
  for (s = rep->iwfmt; *s; ) {  /* traverse the output format */
    if (*s != '%') {            /* copy everything except '%' */
      isr_putc(rep, *s++); n++; continue; }
    t = s++; k = getsd(s,&s);   /* get the number of signif. digits */
    switch (*s++) {             /* evaluate the indicator character */
      case '%': isr_putc(rep, '%'); n++;                   break;
      case 'g': n += isr_numout(rep, wgt,      k);         break;
      case 'w': n += isr_numout(rep, wgt,      k);         break;
      case 'm': n += isr_numout(rep, wgt/supp, k);         break;
      case  0 : --s;            /* print the requested quantity */
      default : isr_putsn(rep, t, k = s-t); n += k; t = s; break;
    }                           /* otherwise copy characters */
  }
  return n;                     /* return the number of characters */
}  /* isr_wgtout() */

/*--------------------------------------------------------------------*/

int isr_tidout (ISREPORT *rep, int tid)
{                               /* --- print a positive integer */
  int  i = BS_INT;              /* loop variable */
  char buf[BS_INT];             /* output buffer */

  assert(rep && (tid >= 0));    /* check the function arguments */
  do {                          /* digit output loop */
    buf[--i] = (tid % 10) +'0'; /* store the next digit and */
    tid /= 10;                  /* remove it from the number */
  } while (tid > 0);            /* while there are more digits */
  fwrite(buf +i, sizeof(char), BS_INT -i, rep->tidfile);
  return BS_INT -i;             /* print the digits and */
}  /* isr_tidout() */

/*----------------------------------------------------------------------
  Generator Filtering Functions
----------------------------------------------------------------------*/
#ifdef ISR_CLOMAX

static unsigned int is_hash (const void *set, int type)
{                               /* --- compute item set hash value */
  int          i;               /* loop variable */
  unsigned int h;               /* computed hash value */
  const int    *p;              /* to access the items */

  assert(set);                  /* check the function argument */
  p = (const int*)set;          /* type the item set pointer */
  h = *p++; i = h >> 3;         /* get the number of items */
  switch (h & 7) {              /* use Duff's device to */
    do {    h = h *251 +*p++;   /* compute the hash code */
    case 7: h = h *251 +*p++;
    case 6: h = h *251 +*p++;
    case 5: h = h *251 +*p++;
    case 4: h = h *251 +*p++;
    case 3: h = h *251 +*p++;
    case 2: h = h *251 +*p++;
    case 1: h = h *251 +*p++;
    case 0: ; } while (--i >= 0);
  }                             /* semicolon is necessary */
  return h;                     /* return the computed hash value */
  /* This hash function treats an item set like a string, that is, */
  /* the hash code depends on the order of the items. This is no   */
  /* drawback, though, because the comparison also requires that   */
  /* the items are in the same order in the item sets to compare.  */
  /* However, an order-independent hash code could be used to make */
  /* the function is_isgen() faster by avoiding recomputations.    */
}  /* is_hash() */

/*--------------------------------------------------------------------*/

static int is_cmp (const void *a, const void *b, void *d)
{                               /* --- compare two item sets */
  int n;                        /* loop variable, number of items */
  int *x, *y;                   /* to access the item sets */

  assert(a && b);               /* check the function arguments */
  x = (int*)a; y = (int*)b;     /* get/type the item set pointers */
  n = *x++;                     /* if the item set sizes differ, */
  if (n != *y++) return 1;      /* the item sets are not equal */
  while (--n >= 0)              /* traverse and compare the items */
    if (x[n] != y[n]) return 1; /* if an item differs, abort */
  return 0;                     /* otherwise the item sets are equal */
  /* Using memcmp() for the comparison is slower, because memcmp() */
  /* also checks the order relationship, not just equality, which, */
  /* however, is all that is needed inside the hash table.         */
}  /* is_cmp() */

/*--------------------------------------------------------------------*/

static int is_isgen (ISREPORT *rep, int item, SUPP_T supp)
{                               /* --- check for a generator */
  int i;                        /* loop variable */
  int *p, *s;                   /* to access table key and data */
  int a, b;                     /* buffers for items (hold-out) */

  assert(rep && (item >= 0));   /* check the function arguments */
  rep->iset[rep->cnt+1] = item; /* store the new item at the end */
  if (rep->cnt > 0) {           /* if the current set is not empty */
    rep->iset[0] = rep->cnt;    /* copy the item set to the buffer */
    p = (int*)memcpy(rep->iset+1, rep->items, rep->cnt *sizeof(int));
    if (rep->mode & ISR_SORT) { /* if item sorting is needed */
      int_qsort(p, rep->cnt+1); /* (due to reordering in search) */
      if (rep->dir < 0) int_reverse(p, rep->cnt+1);
    }                           /* sort the items according to code */
    a = p[i = rep->cnt];        /* note the first hold-out item */
    for (++i; --i >= 0; ) {     /* traverse the items in the set */
      b = p[i]; p[i] = a; a = b;/* get next subset (next hold-out) */
      if (a == item) continue;  /* do not exclude the new item */
      s = (int*)st_lookup(rep->gentab, rep->iset, 0);
      if (!s || (*s == supp))   /* if a subset with one item less */
        break;                  /* is not in the generator repository */
    }                           /* or has the same support, abort */
    if (i >= 0) return 0;       /* if subset was found, no generator */
    memmove(p+1, p, rep->cnt *sizeof(int));
    p[0] = a;                   /* restore the full new item set */
  }                             /* (with the proper item order) */
  rep->iset[0] = rep->cnt+1;    /* store the new item set size */
  i = (rep->cnt+2) *sizeof(int);/* compute the size of the key */
  s = (int*)st_insert(rep->gentab, rep->iset, 0, i, sizeof(int));
  if (!s) return -1;            /* add the new set to the repository */
  *s = supp;                    /* and store its support as the data */
  return 1;                     /* return 'set is a generator' */
}  /* is_isgen() */

#endif
/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

ISREPORT* isr_create (ITEMBASE *base, int mode, int dir,
                      const char *hdr, const char *sep, const char *imp)
{                               /* --- create an item set reporter */
  int        i, k, n, b = 0;    /* loop variables, buffers */
  ISREPORT   *rep;              /* created item set reporter */
  int        len, sum;          /* length of an item name and sum */
  char       *buf;              /* buffer for formated item name */
  const char *name;             /* to traverse the item names */

  assert(base);                 /* check the function arguments */
  if (mode &  ISR_GENERA)       /* generators *or* closed/maximal */
    mode &= ~(ISR_CLOSED|ISR_MAXIMAL|ISR_NOFILTER);
  if (mode & (ISR_CLOSED|ISR_MAXIMAL))
    mode |= ISR_NOEXPAND;       /* make reporting mode consistent */
  n   = ib_cnt(base);           /* get the number of items/trans. */
  rep = (ISREPORT*)malloc(sizeof(ISREPORT) +(n+n+1) *sizeof(char*));
  if (!rep) return NULL;        /* allocate the base structure */
  rep->base    = base;          /* store the item base */
  rep->file    = NULL;          /* clear the output file and its name */
  rep->name    = NULL;          /* and allocate a file write buffer */
  rep->buf     = (char*)malloc(BS_WRITE *sizeof(char));
  if (!rep->buf) { free(rep); return NULL; }
  rep->next    = rep->buf;
  rep->end     = rep->buf +BS_WRITE;
  rep->mode    = mode & MODEMASK;
  rep->rep     = 0;             /* init. the item set counter and */
  rep->min     = 1;             /* the range of item set sizes */
  rep->max     = n;             /* (minimum and maximum size) */
  rep->maxx    = ((mode & (ISR_CLOSED|ISR_MAXIMAL)) && (n < INT_MAX))
               ? n+1 : n;       /* special maximum for isr_xable() */
  rep->smin    = 0;             /* initialize the support range */
  rep->smax    = INT_MAX;       /* (minimum and maximum support) */
  rep->cnt     = rep->pfx = 0;  /* init. the number of items */
  rep->evalfn  = (ISEVALFN*)0;  /* clear add. evaluation function */
  rep->evaldat = NULL;          /* and the corresponding data */
  rep->evalthh = rep->eval = 0; /* clear evaluation and its minimum */
  rep->evaldir = 1;             /* default: threshold is minimum */
  rep->repofn  = (ISREPOFN*)0;  /* clear item set report function */
  rep->repodat = NULL;          /* and the corresponding data */
  rep->tidfile = NULL;          /* clear the transaction id file */
  rep->tidname = NULL;          /* and its name */
  rep->tids    = NULL;          /* clear transaction ids array and */
  rep->tidcnt  = 0;             /* the number of transaction ids */
  rep->tracnt  = 0;             /* set default value for the other */
  rep->miscnt  = 0;             /* transaction ids array variables */
  rep->inames  = (const char**)(rep->pos +n+1);
  memset((void*)rep->inames, 0, n *sizeof(const char*));
  rep->out     = NULL;          /* organize the pointer arrays */
  rep->wgts    = rep->logs = rep->sums = NULL;
  #ifdef ISR_CLOMAX             /* if closed/maximal set filtering */
  rep->sto     = (mode & ISR_MAXONLY) ? 0 : INT_MAX;
  rep->dir     = (dir < 0) ? -1 : 1;
  rep->iset    = NULL;          /* initialize all pointers */
  rep->clomax  = NULL;          /* for an easier abort on failure */
  rep->gentab  = NULL;          /* (can simply call isr_delete()) */
  if ((mode & (ISR_CLOSED|ISR_MAXIMAL|ISR_GENERA))
  && !(mode &  ISR_NOFILTER))   /* if to filter reported item sets */
    b = n+1;                    /* with an internal repository, */
  #endif                        /* get size of extra item set buffer */
  rep->stats = (long*)  malloc((n+1)         *sizeof(long)
                              +(n+1+n+n+1+b) *sizeof(int));
  rep->supps = (SUPP_T*)malloc((n+1)         *sizeof(SUPP_T));
  if (!rep->stats || !rep->supps) { isr_delete(rep, 0); return NULL; }
  memset(rep->stats, 0, (n+1) *sizeof(long) +(n+1) *sizeof(int));
  rep->pxpp  = (int*)(rep->stats +n+1);
  rep->pexs  = rep->pxpp +n+1;  /* allocate memory for the arrays */
  rep->items = rep->pexs += n;  /* and organize and initialize it */
  rep->supps[0] = base->wgt;    /* init. the empty set support */
  if (mode & ISR_WEIGHTS) {     /* if to use item set weights */
    rep->wgts = (double*)calloc((n+1), sizeof(double));
    if (!rep->wgts) { isr_delete(rep, 0); return NULL; }
    rep->wgts[0] = base->wgt;   /* create a floating point array */
  }                             /* and store the empty set support */
  if (mode & ISR_LOGS) {        /* if to compute logarithms of freqs. */
    rep->logs = (double*)calloc((n+n+1), sizeof(double));
    if (!rep->logs) { isr_delete(rep, 0); return NULL; }
    rep->sums = rep->logs +n;   /* allocate the needed arrays */
    for (i = 0; i < n; i++)     /* compute logarithms of item freqs. */
      rep->logs[i] = log((double)ib_getfrq(base, i));
    rep->logwgt  = log((double)base->wgt);
    rep->sums[0] = 0;           /* store the log of the total weight */
  }                             /* and init. the sum of logarithms */
  if ((n > 0)                   /* if item names are integers */
  &&  (ib_mode(base) & IB_INTNAMES)) {
    for (i = sum = 0; i < n; i++) {
      k = ib_int(base, i);      /* traverse the integer values */
      if (k < 0) { k = -k; sum++; }
      while ((k /= 10) > 0) sum++;
      sum++;                    /* determine the total size */
    }                           /* of the formatted integers */
    buf = (char*)malloc((sum +n) *sizeof(char));
    if (!buf) { isr_delete(rep, 0); return NULL; }
    for (i = 0; i < n; i++) {   /* traverse the items again and */
      rep->inames[i] = buf;     /* print the integer numbers */
      buf += sprintf(buf, "%d", ib_int(base, i)) +1;
    } }                         /* store the integer names */
  else {                        /* if item names are strings */
    for (i = sum = 0; i < n; i++) {
      name = ib_name(base, i);  /* traverse the items and their names */
      if (!(mode & ISR_SCAN))   /* if to use the items names directly */
        sum += strlen(name);    /* sum their string lengths */
      else {                    /* if name formatting may be needed */
        sum += k = scn_fmtlen(name, &len);
        if (k > len) {          /* if name formatting is needed */
          buf = (char*)malloc((k+1) *sizeof(char));
          if (buf) scn_format(buf, name, 0);
          name = buf;           /* format the item name */
        }                       /* (quote certain characters) */
      }                         /* and replace the original name */
      rep->inames[i] = name;    /* store the (formatted) item name */
      if (!name) { isr_delete(rep, 0); return NULL; }
    }                           /* check for proper name copying */
  }                             /* (in case it was formatted) */
  rep->inames[n] = NULL;        /* store a sentinel after the names */
  if (!hdr) hdr = "";           /* get default header      if needed */
  if (!sep) sep = " ";          /* get default separator   if needed */
  if (!imp) imp = " <- ";       /* get default implication if needed */
  i = strlen(hdr); k = strlen(sep);
  sum += i +(n-1) *k +1;        /* compute the output buffer size and */
  len  = i +k +strlen(imp) +3;  /* space for header/separator/implic. */
  buf  = (char*)malloc((sum+len) *sizeof(char));
  if (!buf) { isr_delete(rep, 0); return NULL; }
  rep->out    = strcpy(buf,hdr);/* allocate the output buffer */
  rep->pos[0] = buf +i;         /* and copy the header into it */
  rep->hdr    = strcpy(buf += sum, hdr);
  rep->sep    = strcpy(buf += i+1, sep);
  rep->imp    = strcpy(buf += k+1, imp);
  rep->iwfmt  = ":%w";          /* note header/separator/implication */
  rep->format = "  (%a)";       /* and formats for weights and info. */
  rep->fast   = -1;             /* default: only count the item sets */
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  if (b <= 0) return rep;       /* if to report all item sets, abort */
  rep->iset = rep->items +n+1;  /* set the second item set buffer */
  if (mode & ISR_GENERA) {      /* if to filter for generators, */
    n = 1024*1024-1;            /* create an item set hash table */
    rep->gentab = st_create(n, 0, is_hash, is_cmp, NULL, (OBJFN*)0);
    if (!rep->gentab) { isr_delete(rep, 0); return NULL; } }
  else {                        /* if to filter for closed/maximal */
    rep->clomax = cm_create(dir, n);
    if (!rep->clomax) { isr_delete(rep, 0); return NULL; }
  }                             /* create a closed/maximal filter */
  #endif
  return rep;                   /* return created item set reporter */
}  /* isr_create() */

/*--------------------------------------------------------------------*/

int isr_delete (ISREPORT *rep, int mode)
{                               /* --- delete an item set reporter */
  int i, k;                     /* loop variable, buffers */

  assert(rep);                  /* check the function argument */
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  if (rep->clomax) cm_delete(rep->clomax);
  if (rep->gentab) st_delete(rep->gentab);
  #endif                        /* delete the closed/maximal filter */
  if (rep->out) free(rep->out); /* delete the output buffer */
  if (ib_mode(rep->base) & IB_INTNAMES) {
    if (rep->inames[0]) free((void*)rep->inames[0]); }
  else {                        /* delete the integer names */
    for (i = 0; rep->inames[i]; i++)
      if (rep->inames[i] != ib_name(rep->base, i))
        free((void*)rep->inames[i]);
  }                             /* delete all cloned names */
  if (rep->logs)  free(rep->logs);   /* delete the arrays */
  if (rep->wgts)  free(rep->wgts);   /* (if they are present) */
  if (rep->supps) free(rep->supps);
  if (rep->stats) free(rep->stats);  /* delete the item base */
  if (mode & ISR_DELISET) ib_delete(rep->base);
  k = (mode & ISR_FCLOSE) ? isr_tidclose(rep) : 0;
  i = (mode & ISR_FCLOSE) ? isr_close(rep)    : 0;
  free(rep->buf);               /* delete the file write buffer */
  free(rep);                    /* delete the base structure */
  return (i) ? i : k;           /* return file closing result */
}  /* isr_delete() */

/*--------------------------------------------------------------------*/

int isr_open (ISREPORT *rep, FILE *file, const char *name)
{                               /* --- open an output file */
  assert(rep);                  /* check the function arguments */
  if (file)                     /* if a file is given, */
    rep->name = name;           /* store the file name */
  else if (! name) {            /* if no name is given */
    file = NULL;   rep->name = "<null>"; }
  else if (!*name) {            /* if an empty name is given */
    file = stdout; rep->name = "<stdout>"; }
  else {                        /* if a proper name is given */
    file = fopen(rep->name = name, "w");
    if (!file) return -2;       /* open file with given name */
  }                             /* and check for an error */
  rep->file = file;             /* store the new output file */
  fastchk(rep);                 /* check for fast output */
  return 0;                     /* return 'ok' */
}  /* isr_open() */

/*--------------------------------------------------------------------*/

int isr_close (ISREPORT *rep)
{                               /* --- close the output file */
  int r;                        /* result of fclose()/fflush() */

  assert(rep);                  /* check the function arguments */
  if (!rep->file) return 0;     /* check for an output file */
  isr_flush(rep);               /* flush the write buffer */
  r = ((rep->file == stdout) || (rep->file == stderr))
    ? fflush(rep->file) : fclose(rep->file);
  rep->file = NULL;             /* close the current output file */
  fastchk(rep);                 /* check for fast output */
  return r;                     /* return the result of fclose() */
}  /* isr_close() */

/*--------------------------------------------------------------------*/

void isr_reset (ISREPORT *rep)
{                               /* --- reset the output counters */
  rep->rep = 0;                 /* reinit. number of reported sets */
  memset(rep->stats, 0, ((long*)rep->pxpp -rep->stats) *sizeof(long));
}  /* isr_reset() */

/*--------------------------------------------------------------------*/

void isr_setsize (ISREPORT *rep, int min, int max)
{                               /* --- set size range for item set */
  assert(rep                    /* check the function arguments */
     && (min >= 0) && (max >= min));
  rep->min  = min;              /* store the minimum and maximum */
  rep->max  = max;              /* size of an item set to report */
  rep->maxx = ((rep->mode & (ISR_CLOSED|ISR_MAXIMAL))
            && (max < INT_MAX)) ? max+1 : max;
  fastchk(rep);                 /* check for fast output */
}  /* isr_setsize() */

/*--------------------------------------------------------------------*/

void isr_setsupp (ISREPORT *rep, int min, int max)
{                               /* --- set support range for item set */
  assert(rep                    /* check the function arguments */
     && (min >= 0) && (max >= min));
  rep->smin = min;              /* store the minimum and maximum */
  rep->smax = max;              /* support of an item set to report */
}  /* isr_setsupp() */

/*--------------------------------------------------------------------*/

void isr_seteval (ISREPORT *rep, ISEVALFN evalfn, void *data,
                  int dir, double thresh)
{                               /* --- set evaluation function */
  assert(rep);                  /* check the function argument */
  rep->evalfn  = evalfn;        /* store the evaluation function, */
  rep->evaldat = data;          /* the corresponding user data, */
  rep->evaldir = (dir >= 0) ? +1 : -1;  /* the evaluation direction */
  rep->evalthh = rep->evaldir *thresh;  /* and the threshold value  */
  fastchk(rep);                 /* check for fast output */
}  /* isr_seteval() */

/*--------------------------------------------------------------------*/

void isr_setrepo (ISREPORT *rep, ISREPOFN repofn, void *data)
{                               /* --- set evaluation function */
  assert(rep);                  /* check the function argument */
  rep->repofn  = repofn;        /* store the reporting function and */
  rep->repodat = data;          /* the corresponding user data */
  fastchk(rep);                 /* check for fast output */
}  /* isr_setrepo() */

/*--------------------------------------------------------------------*/

int isr_tidopen (ISREPORT *rep, FILE *file, const char *name)
{                               /* --- set/open trans. id output file */
  assert(rep);                  /* check the function arguments */
  if (file) {                   /* if a file is given directly, */
    if      (name)           rep->tidname = name; /* store name */
    else if (file == stdout) rep->tidname = "<stdout>";
    else if (file == stderr) rep->tidname = "<stderr>";
    else                     rep->tidname = "<unknown>"; }
  else if (! name) {            /* if no name is given */
    file = NULL;             rep->tidname = "<null>"; }
  else if (!*name) {            /* if an empty name is given */
    file = stdout;           rep->tidname = "<stdout>"; }
  else {                        /* if a proper name is given */
    file = fopen(rep->tidname = name, "w");
    if (!file) return -2;       /* open file with given name */
  }                             /* and check for an error */
  rep->tidfile = file;          /* store the new output file */
  fastchk(rep);                 /* check for fast output */
  return 0;                     /* return 'ok' */
}  /* isr_tidopen() */

/*--------------------------------------------------------------------*/

int isr_tidclose (ISREPORT *rep)
{                               /* --- close trans. id output file */
  int r;                        /* result of fclose() */

  assert(rep);                  /* check the function arguments */
  if (!rep->tidfile) return 0;  /* check for an output file */
  r = ((rep->tidfile == stdout) || (rep->tidfile == stderr))
    ? fflush(rep->tidfile) : fclose(rep->tidfile);
  rep->tidfile = NULL;          /* close the current output file */
  fastchk(rep);                 /* check for fast output */
  return r;                     /* return the result of fclose() */
}  /* isr_tidclose() */

/*--------------------------------------------------------------------*/

void isr_tidcfg (ISREPORT *rep, int tracnt, int miscnt)
{                               /* --- configure trans. id output */
  rep->tracnt = tracnt;         /* note the number of transactions */
  rep->miscnt = miscnt;         /* and the accepted number of */
}  /* isr_tidcfg() */           /* missing items */

/*--------------------------------------------------------------------*/

int isr_add (ISREPORT *rep, int item, SUPP_T supp)
{                               /* --- add an item (only support) */
  assert(rep && (item >= 0)     /* check the function arguments */
  &&    (item < ib_cnt(rep->base)) && !isr_uses(rep, item));
  // if (supp < rep->smin) return 0;
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  if      (rep->clomax) {       /* if a closed/maximal filter exists */
    int r = cm_add(rep->clomax, item, supp);
    if (r <= 0) return r; }     /* add the item to the c/m filter */
  else if (rep->gentab) {       /* if a generator filter exists */
    int r = is_isgen(rep, item, supp);
    if (r <= 0) return r;       /* add item set to the gen. filter */
  }                             /* check if item needs processing */
  #endif
  rep->pxpp [item] |= INT_MIN;  /* mark the item as used */
  rep->items[  rep->cnt] = item;/* store the item and its support */
  rep->supps[++rep->cnt] = supp;/* clear the perfect ext. counter */
  rep->pxpp [  rep->cnt] &= INT_MIN;
  return rep->cnt;              /* return the new number of items */
}  /* isr_add() */

/*--------------------------------------------------------------------*/

int isr_addnc (ISREPORT *rep, int item, SUPP_T supp)
{                               /* --- add an item (only support) */
  assert(rep && (item >= 0)     /* check the function arguments */
  &&    (item < ib_cnt(rep->base)) && !isr_uses(rep, item));
  // if (supp < rep->smin) return 0;
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  if (rep->clomax) {            /* if a closed/maximal filter exists */
    int r = cm_addnc(rep->clomax, item, supp);
    if (r <= 0) return r;       /* add the item to the c/m filter */
  }                             /* check only for a memory error */
  #endif
  rep->pxpp [item] |= INT_MIN;  /* mark the item as used */
  rep->items[  rep->cnt] = item;/* store the item and its support */
  rep->supps[++rep->cnt] = supp;/* clear the perfect ext. counter */
  rep->pxpp [  rep->cnt] &= INT_MIN;
  return rep->cnt;              /* return the new number of items */
}  /* isr_addnc() */

/* In contrast to isr_add(), the function isr_addnc() does not check */
/* whether the extended prefix possesses a perfect extension.        */

/*--------------------------------------------------------------------*/

int isr_addx (ISREPORT *rep, int item, SUPP_T supp, double wgt)
{                               /* --- add an item (support & weight) */
  assert(rep && (item >= 0)     /* check the function arguments */
  &&    (item < ib_cnt(rep->base)) && !isr_uses(rep, item));
  // if (supp < rep->smin) return 0;
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  if      (rep->clomax) {       /* if a closed/maximal filter exists */
    int r = cm_add(rep->clomax, item, supp);
    if (r <= 0) return r; }     /* add the item to the c/m filter */
  else if (rep->gentab) {       /* if a generator filter exists */
    int r = is_isgen(rep, item, supp);
    if (r <= 0) return r;       /* add item set to the gen. filter */
  }                             /* check if item needs processing */
  #endif
  rep->pxpp [item] |= INT_MIN;  /* mark the item as used */
  rep->items[  rep->cnt] = item;/* store the item and its support */
  rep->supps[++rep->cnt] = supp;/* as well as its weight */
  rep->wgts [  rep->cnt] = wgt; /* clear the perfect ext. counter */
  rep->pxpp [  rep->cnt] &= INT_MIN;
  return rep->cnt;              /* return the new number of items */
}  /* isr_addx() */

/*--------------------------------------------------------------------*/

int isr_addpex (ISREPORT *rep, int item)
{                               /* --- add a perfect extension */
  assert(rep && (item >= 0)     /* check the function arguments */
  &&    (item < ib_cnt(rep->base)));
  if ((rep->pxpp[item] < 0)     /* if the item is already in use */
  ||  (rep->mode & ISR_GENERA)) /* or to filter for generators, */
    return 0;                   /* perfect extensions are ignored */
  rep->pxpp[item] |= INT_MIN;   /* mark the item as used */
  *--rep->pexs = item;          /* store the added item and */
  rep->pxpp[rep->cnt]++;        /* count it for the current prefix */
  return rep->items -rep->pexs; /* return the number of perf. exts. */
}  /* isr_addpex() */

/*--------------------------------------------------------------------*/

int isr_addpexpk (ISREPORT *rep, int bits)
{                               /* --- add a perfect extension */
  int i;                        /* loop variable/item */

  assert(rep);                  /* check the function arguments */
  bits &= ~INT_MIN;             /* traverse the set bits */
  for (i = 0; (unsigned int)(1 << i) <= (unsigned int)bits; i++) {
    if (((bits & (1 << i)) == 0)/* if the bit is not set */
    || (rep->pxpp[i] < 0)       /* or the item is already in use */
    || (rep->mode & ISR_GENERA))/* or to filter for generators, */
      continue;                 /* perfect extensions are ignored */
    rep->pxpp[i] |= INT_MIN;    /* mark the item as used */
    *--rep->pexs = i;           /* store the added item and */
    rep->pxpp[rep->cnt]++;      /* count it for the current prefix */
  }
  return rep->items -rep->pexs; /* return the number of perf. exts. */
}  /* isr_addpexpk() */

/*--------------------------------------------------------------------*/

int isr_remove (ISREPORT *rep, int n)
{                               /* --- remove one or more items */
  int i;                        /* loop variable, buffer for an item */

  assert(rep                    /* check the function arguments */
  &&    (n >= 0) && (n <= rep->cnt));
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  if (rep->clomax)              /* if a closed/maximal filter exists, */
    cm_remove(rep->clomax, n);  /* remove the same number of items */
  #endif                        /* from this filter */
  while (--n >= 0) {            /* traverse the items to remove */
    for (i = rep->pxpp[rep->cnt] & ~INT_MIN; --i >= 0; )
      rep->pxpp[*rep->pexs++] &= ~INT_MIN;
    i = rep->items[--rep->cnt]; /* traverse the item to remove */
    rep->pxpp[i] &= ~INT_MIN;   /* (current item and perfect exts.) */
  }                             /* and remove their "in use" markers */
  if (rep->cnt < rep->pfx)      /* if too few items are left, */
    rep->pfx = rep->cnt;        /* reduce the valid prefix */
  return rep->cnt;              /* return the new number of items */
}  /* isr_remove() */

/*--------------------------------------------------------------------*/

double isr_logrto (ISREPORT *rep, void *data)
{                               /* --- logarithm of support ratio */
  assert(rep);                  /* check the function arguments */
  return (log(rep->supps[rep->cnt])
             -rep->sums [rep->cnt] +(rep->cnt-1)*rep->logwgt) /LN_2;
}  /* isr_logrto() */

/* Evaluate an itemset by the logarithm of the quotient of the actual */
/* support of an item set and the support that is expected under full */
/* independence of the items (product of item probabilities times the */
/* total transaction weight). 'data' is needed for the interface.     */

/*--------------------------------------------------------------------*/

double isr_logsize (ISREPORT *rep, void *data)
{                               /* --- logarithm of support quotient */
  assert(rep);                  /* check the function arguments */
  return (log(rep->supps[rep->cnt])
             -rep->sums [rep->cnt] +(rep->cnt-1)*rep->logwgt)
       / (rep->cnt *LN_2);      /* divide by item set size */
}  /* isr_logsize() */

/*--------------------------------------------------------------------*/

double isr_sizewgt (ISREPORT *rep, void *data)
{                               /* --- item set size times weight */
  assert(rep);                  /* check the function arguments */
  return rep->wgts[rep->cnt] *rep->cnt;
}  /* isr_sizewgt() */

/* Evaluate an item set by the product of size and weight in order to */
/* favor large item sets and thus to compensate anti-monotone weights.*/

/*--------------------------------------------------------------------*/

double isr_wgtsize (ISREPORT *rep, void *data)
{                               /* --- item set weight / size */
  assert(rep);                  /* check the function arguments */
  return (rep->cnt > 0) ? rep->wgts[rep->cnt] /rep->cnt : 0;
}  /* isr_wgtsize() */

/*--------------------------------------------------------------------*/

double isr_wgtsupp (ISREPORT *rep, void *data)
{                               /* --- item set weight / size */
  double s;                     /* buffer for support */
  assert(rep);                  /* check the function arguments */
  return ((s = rep->supps[rep->cnt]) > 0) ? rep->wgts[rep->cnt] /s : 0;
}  /* isr_wgtsupp() */

/*--------------------------------------------------------------------*/

static void fastout (ISREPORT *rep, int n)
{                               /* --- fast output of an item set */
  char       *s;                /* to traverse the output buffer */
  const char *name;             /* to traverse the item names */

  assert(rep);                  /* check the function argument */
  rep->stats[rep->cnt]++;       /* count the reported item set */
  rep->rep++;                   /* (for its size and overall) */
  s = rep->pos[rep->pfx];       /* get the position for appending */
  while (rep->pfx < rep->cnt) { /* traverse the additional items */
    if (rep->pfx > 0)           /* if this is not the first item */
      for (name = rep->sep; *name; )
        *s++ = *name++;         /* copy the item separator */
    for (name = rep->inames[rep->items[rep->pfx]]; *name; )
      *s++ = *name++;           /* copy the item name to the buffer */
    rep->pos[++rep->pfx] = s;   /* compute and record new position */
  }                             /* for appending the next item */
  isr_putsn(rep, rep->out, s-rep->out);  /* print item set */
  isr_putsn(rep, rep->info, rep->size);  /* and its support */
  while (n > 0) {               /* traverse the perfect extensions */
    rep->items[rep->cnt++] = rep->pexs[--n];
    fastout(rep, n);            /* add the next perfect extension, */
    rep->pfx = --rep->cnt;      /* recursively report supersets, */
  }                             /* and remove the item again */
}  /* fastout() */

/*--------------------------------------------------------------------*/

static void output (ISREPORT *rep)
{                               /* --- output an item set */
  int        i, k;              /* loop variable, flag */
  int        min;               /* minimum number of items */
  char       *s;                /* to traverse the output buffer */
  const char *name;             /* to traverse the item names */
  double     sum;               /* to compute the logarithm sums */

  assert(rep                    /* check the function arguments */
  &&    (rep->cnt >= rep->min)
  &&    (rep->cnt <= rep->max));
  if (!rep->evalfn) {           /* if no  evaluation function is given */
    if (rep->wgts)              /* use the weight as evaluation */
      rep->eval = rep->wgts[rep->cnt]; }
  else {                        /* if an evaluation function is given */
    if (rep->logs) {            /* if to compute sums of logarithms */
      sum = rep->sums[rep->pfx];/* get the valid sum for a prefix */
      for (i = rep->pfx; i < rep->cnt; ) {
        sum += rep->logs[rep->items[i]];
        rep->sums[++i] = sum;   /* traverse the additional items */
      }                         /* and add the logarithms of */
    }                           /* their individual frequencies */
    rep->eval = rep->evalfn(rep, rep->evaldat);
    if (rep->evaldir *rep->eval < rep->evalthh)
      return;                   /* if the item set does not qualify, */
  }                             /* abort the output function */
  rep->stats[rep->cnt]++;       /* count the reported item set */
  rep->rep++;                   /* (for its size and overall) */
  if (rep->repofn)              /* call reporting function if given */
    rep->repofn(rep, rep->repodat);
  if (!rep->file) return;       /* check for an output file */
  s = rep->pos[rep->pfx];       /* get the position for appending */
  while (rep->pfx < rep->cnt) { /* traverse the additional items */
    if (rep->pfx > 0)           /* if this is not the first item */
      for (name = rep->sep; *name; )
        *s++ = *name++;         /* copy the item separator */
    for (name = rep->inames[rep->items[rep->pfx]]; *name; )
      *s++ = *name++;           /* copy the item name to the buffer */
    rep->pos[++rep->pfx] = s;   /* compute and record new position */
  }                             /* for appending the next item */
  isr_putsn(rep, rep->out, s -rep->out);
  isr_sinfo(rep, rep->supps[rep->cnt],
            (rep->wgts) ? rep->wgts[rep->cnt] : 0, rep->eval);
  isr_putc(rep, '\n');          /* print the item set information */
  if (!rep->tidfile || !rep->tids) /* check whether to report */
    return;                        /* a list of transaction ids */
  if      (rep->tidcnt > 0) {   /* if tids are in ascending order */
    for (i = 0; i < rep->tidcnt; i++) {
      if (i > 0) fputs(rep->sep, rep->tidfile);
      isr_tidout(rep, rep->tids[i]+1);
    } }                         /* report the transaction ids */
  else if (rep->tidcnt < 0) {   /* if tids are in descending order */
    for (i = -rep->tidcnt; --i >= 0; ) {
      isr_tidout(rep, rep->tids[i]+1);
      if (i > 0) fputs(rep->sep, rep->tidfile);
    } }                         /* report the transaction ids */
  else if (rep->tracnt > 0) {   /* if item occurrence counters */
    min = rep->cnt -rep->miscnt;/* traverse all transaction ids */
    for (i = k = 0; i < rep->tracnt; i++) {
      if (rep->tids[i] < min)   /* skip all transactions that */
        continue;               /* do not contain enough items */
      if (k++ > 0) fputs(rep->sep, rep->tidfile);
      isr_tidout(rep, i+1);     /* print the transaction identifier */
      if (rep->miscnt <= 0) continue;
      fputc(':', rep->tidfile); /* print an item counter separator */
      isr_tidout(rep, rep->tids[i]);
    }                           /* print number of contained items */
  }
  fputc('\n', rep->tidfile);    /* terminate the transaction id list */
}  /* output() */

/*--------------------------------------------------------------------*/

static void report (ISREPORT *rep, int n)
{                               /* --- recursively report item sets */
  assert(rep && (n >= 0));      /* check the function arguments */
  if (rep->cnt >= rep->min)     /* if item set has minimum size, */
    output(rep);                /* report the current item set */
  while (n > 0) {               /* traverse the perfect extensions */
    rep->items[rep->cnt++] = rep->pexs[--n];
    if ((rep->cnt+n >= rep->min)   /* if a valid size can be reached */
    &&  (rep->cnt   <  rep->max))  /* (in the interval [min, max]), */
      report(rep, n);              /* recursively report supersets */
    if (--rep->cnt < rep->pfx)  /* remove the current item again */
      rep->pfx = rep->cnt;      /* and adapt the valid prefix */
  }
}  /* report() */

/*--------------------------------------------------------------------*/

int isr_report (ISREPORT *rep)
{                               /* --- report the current item set */
  int    n, k, z;               /* number of perfect exts., buffer */
  long   m, c;                  /* buffers for item set counting */
  double w;                     /* buffer for an item set weight */
  #ifdef ISR_CLOMAX             /* if closed/maximal filtering */
  SUPP_T s, r;                  /* support buffers */
  int    *items;                /* item set for prefix tree update */
  #endif

  assert(rep);                  /* check the function argument */
  n = rep->items -rep->pexs;    /* get number of perfect extensions */
  #ifdef ISR_CLOMAX             /* closed/maximal filtering support */
  if (rep->clomax) {            /* if a closed/maximal filter exists */
    s = rep->supps[rep->cnt];   /* get the support of the item set */
    r = cm_supp(rep->clomax);   /* and the maximal known support */
    if (r >= s)        return 0;/* check if item set is not closed */
    if (r >= rep->sto) return 0;/* check whether to store item set */
    k = rep->cnt +n;            /* compute the total number of items */
    if (n <= 0)                 /* if there are no perfect extensions */
      items = rep->items;       /* the items can be used directly */
    else {                      /* if there are perfect extensions, */
      items = (int*)memcpy(rep->iset, rep->pexs, k *sizeof(int));
      int_qsort(items, k);      /* copy and sort the items in the set */
      if (rep->dir < 0) int_reverse(items, k);
    }                           /* respect the repository item order */
    if (cm_update(rep->clomax, items, k, s) < 0)
      return -1;                /* add the item set to the filter */
    if ((rep->mode & ISR_MAXIMAL) && (r >= 0))
      return  0;                /* check for a non-maximal item set */
  }                             /* (if the known support is > 0) */
  #endif
  if ((rep->cnt   > rep->max)   /* if the item set is too large or */
  ||  (rep->cnt+n < rep->min))  /* the minimum size cannot be reached */
    return 0;                   /* with prefect extensions, abort */
  if (rep->fast < 0) {          /* if just to count the item sets */
    /* if no output is produced and no item sets can be filtered out, */
    /* compute the number of item sets in the perfect ext. hypercube. */
    if (rep->mode & ISR_NOEXPAND) {
      rep->stats[rep->cnt+n]++; /* if not to expand perfect exts., */
      rep->rep++; return 1;     /* count one item set with all */
    }                           /* perfect extensions included */
    m = 0; z = rep->cnt;        /* init. the item set counter */
    if (z >= rep->min) {        /* count the current item set */
      rep->stats[z]++; m++; }   /* (for its size and overall) */
    for (k = c = 1; (k <= n) && (++z <= rep->max); k++) {
      c = (c *(n-k+1)) /k;      /* compute n choose k for 1 <= k <= n */
      if (z >= rep->min) { rep->stats[z] += c; m += c; }
    }                           /* (n choose k is the number of */
    rep->rep += m; return m;    /* item sets of size rep->cnt +k) */
  }                             /* return the number of item sets */
  /* It is debatable whether this way of handling perfect extensions  */
  /* in case no output is produced is acceptable for fair benchmarks, */
  /* since the sets in the hypercube are not explicitly generated.    */
  if (rep->fast)                /* format support for fast output */
    rep->size = sprintf(rep->info, " (%d)\n", rep->supps[rep->cnt]);
  if (rep->mode & ISR_NOEXPAND){/* if not to expand perfect exts. */
    k = rep->cnt +n;            /* if all perfext extensions make */
    if (k > rep->max) return 0; /* the item set too large, abort */
    rep->supps[k] = rep->supps[rep->cnt];
    if (rep->wgts) rep->wgts[k] = rep->wgts[rep->cnt];
    for (k = n; --k >= 0; )     /* add all perfect extensions */
      rep->items[rep->cnt++] = rep->pexs[k];
    if (rep->fast) fastout(rep, 0); /* report the expanded set */
    else           output (rep);    /* (fast or normal output) */
    rep->cnt -= n; return 1;    /* remove the perfect extensions */
  }                             /* and abort the function */
  m = rep->rep;                 /* note the number of reported sets */
  if (rep->fast)                /* if fast output is possible, */
    fastout(rep, n);            /* report item sets recursively */
  else {                        /* if fast output is not possible */
    z = rep->supps[rep->cnt];   /* set support for pex. hypercube */
    for (k = 0; ++k <= n; ) rep->supps[rep->cnt+k] = z;
    if (rep->wgts) {            /* if there are item set weights, */
      w = rep->wgts[rep->cnt];  /* set weights for pex. hypercube */
      for (k = 0; ++k <= n; ) rep->wgts[rep->cnt+k] = w;
    }                           /* (support/weight is the same) */
    report(rep, n);             /* recursively add perfect exts. and */
  }                             /* report the resulting item sets */
  #ifndef NDEBUG                /* in debug mode */
  isr_flush(rep);               /* flush the output buffer */
  #endif                        /* after every item set */
  return rep->rep -m;           /* return number of rep. item sets */
}  /* isr_report() */

/*--------------------------------------------------------------------*/

int isr_reportx (ISREPORT *rep, int *tids, int n)
{                               /* --- report the current item set */
  assert(rep);                  /* check the function arguments */
  rep->tids   = tids;           /* store the transaction id array */
  rep->tidcnt = n;              /* and the number of transaction ids */
  n = isr_report(rep);          /* report the current item set */
  rep->tids   = NULL;           /* clear the transaction id array */
  return n;                     /* return number of rep. item sets */
}  /* isr_reportx() */

/*--------------------------------------------------------------------*/

void isr_direct (ISREPORT *rep, const int *items, int n,
                 SUPP_T supp, double wgt, double eval)
{                               /* --- report an item set */
  int c;                        /* buffer for the item counter */

  assert(rep                    /* check the function arguments */
  &&    (items || (n <= 0)) && (supp >= 0));
  if ((n < rep->min) || (n > rep->max))
    return;                     /* check the item set size */
  rep->stats[n]++;              /* count the reported item set */
  rep->rep++;                   /* (for its size and overall) */
  if (!rep->file) return;       /* check for an output file */
  c = rep->cnt; rep->cnt = n;   /* note the number of items */
  isr_puts(rep, rep->hdr);      /* print the record header */
  if (n > 0)                    /* print the first item */
    isr_puts(rep, rep->inames[*items++]);
  while (--n > 0) {             /* traverse the remaining items */
    isr_puts(rep, rep->sep);    /* print an item separator */
    isr_puts(rep, rep->inames[*items++]);
  }                             /* print the next item */
  isr_sinfo(rep, supp, wgt, eval);
  isr_putc(rep, '\n');          /* print the item set information */
  rep->cnt = c;                 /* restore the number of items */
}  /* isr_direct() */

/*--------------------------------------------------------------------*/

void isr_directx (ISREPORT *rep, const int *items, int n,
                  const double *iwgts,
                  SUPP_T supp, double wgt, double eval)
{                               /* --- report an item set */
  int c;                        /* buffer for the item counter */

  assert(rep                    /* check the function arguments */
  &&    (items || (n <= 0)) && (supp >= 0));
  if ((n < rep->min) || (n > rep->max))
    return;                     /* check the item set size */
  rep->stats[n]++;              /* count the reported item set */
  rep->rep++;                   /* (for its size and overall) */
  if (!rep->file) return;       /* check for an output file */
  c = rep->cnt; rep->cnt = n;   /* note the number of items */
  isr_puts(rep, rep->hdr);      /* print the record header */
  if (n > 0) {                  /* if at least one item */
    isr_puts(rep, rep->inames[*items]);
    isr_wgtout(rep, supp, *iwgts);
  }                             /* print first item and item weight */
  while (--n > 0) {             /* traverse the remaining items */
    isr_puts(rep, rep->sep);    /* print an item separator */
    isr_puts(rep, rep->inames[*++items]);
    isr_wgtout(rep, supp, *++iwgts);
  }                             /* print next item and item weight */
  isr_sinfo(rep, supp, wgt, eval);
  isr_putc(rep, '\n');          /* print the item set information */
  rep->cnt = c;                 /* restore the number of items */
}  /* isr_directx() */

/*--------------------------------------------------------------------*/

void isr_rule (ISREPORT *rep, const int *items, int n,
               SUPP_T supp, SUPP_T body, SUPP_T head, double eval)
{                               /* --- report an association rule */
  int c;                        /* buffer for the item counter */

  assert(rep                    /* check the function arguments */
  &&     items && (n > 0) && (supp >= 0));
  if ((n < rep->min) || (n > rep->max))
    return;                     /* check the item set size */
  rep->stats[n]++;              /* count the reported rule */
  rep->rep++;                   /* (for its size and overall) */
  if (!rep->file) return;       /* check for an output file */
  c = rep->cnt; rep->cnt = n;   /* note the number of items */
  isr_puts(rep, rep->hdr);      /* print the record header */
  isr_puts(rep, rep->inames[*items++]);
  isr_puts(rep, rep->imp);      /* print the rule head and imp. sign */
  if (--n > 0)                  /* print the first item in body */
    isr_puts(rep, rep->inames[*items++]);
  while (--n > 0) {             /* traverse the remaining items */
    isr_puts(rep, rep->sep);    /* print an item separator */
    isr_puts(rep, rep->inames[*items++]);
  }                             /* print the next item */
  isr_rinfo(rep, supp, body, head, eval);
  isr_putc(rep, '\n');          /* print the item set information */
  rep->cnt = c;                 /* restore the number of items */
}  /* isr_rule() */

/*--------------------------------------------------------------------*/

void isr_prstats (ISREPORT *rep, FILE *out)
{                               /* --- print item set statistics */
  int i, n;                     /* loop variables */

  assert(rep && out);           /* check the function arguments */
  fprintf(out, "all: %ld\n", rep->rep);
  for (n = (long*)rep->pxpp -rep->stats; --n >= 0; )
    if (rep->stats[n] != 0) break;
  for (i = 0; i <= n; i++)      /* print set counters per set size */
    fprintf(out, "%3d: %ld\n", i, rep->stats[i]);
}  /* isr_prstats() */

/*--------------------------------------------------------------------*/

int isr_sinfo (ISREPORT *rep, SUPP_T supp, double wgt, double eval)
{                               /* --- print item set information */
  int        k, n = 0;          /* number of decimals, char. counter */
  double     smax, wmax;        /* maximum support and weight */
  const char *s, *t;            /* to traverse the format */

  assert(rep);                  /* check the function arguments */
  if (!rep->format || !rep->file)
    return 0;                   /* check for a given format and file */
  smax = rep->supps[0];         /* get maximum support and */
  if (smax <= 0) smax = 1;      /* avoid divisions by zero */
  wmax = (rep->wgts) ? rep->wgts[0] : smax;
  if (wmax <= 0) wmax = 1;      /* get maximum weight (if available) */
  for (s = rep->format; *s; ) { /* traverse the output format */
    if (*s != '%') {            /* copy everything except '%' */
      isr_putc(rep, *s++); n++; continue; }
    t = s++; k = getsd(s, &s);  /* get the number of signif. digits */
    switch (*s++) {             /* evaluate the indicator character */
      case '%': isr_putc(rep, '%'); n++;                   break;
      case 'i': n += isr_intout(rep,      rep->cnt);       break;
      case 'n': case 'd':
      #define double 0
      #define int    1
      #if SUPP_T==int
      case 'a': n += isr_intout(rep,      supp);           break;
      #else
      case 'a': n += isr_numout(rep,      supp,       k);  break;
      #endif
      #undef int
      #undef double
      case 's': n += isr_numout(rep,      supp/smax,  k);  break;
      case 'S': n += isr_numout(rep, 100*(supp/smax), k);  break;
      case 'x': n += isr_numout(rep,      supp/smax,  k);  break;
      case 'X': n += isr_numout(rep, 100*(supp/smax), k);  break;
      case 'w': n += isr_numout(rep,      wgt,        k);  break;
      case 'W': n += isr_numout(rep, 100* wgt,        k);  break;
      case 'r': n += isr_numout(rep,      wgt /wmax,  k);  break;
      case 'R': n += isr_numout(rep, 100*(wgt /wmax), k);  break;
      case 'z': n += isr_numout(rep,      wgt *smax,  k);  break;
      case 'e': n += isr_numout(rep,      eval,       k);  break;
      case 'E': n += isr_numout(rep, 100* eval,       k);  break;
      case 'p': n += isr_numout(rep,      eval,       k);  break;
      case 'P': n += isr_numout(rep, 100* eval,       k);  break;
      case  0 : --s;            /* print the requested quantity */
      default : isr_putsn(rep, t, k = s-t); n += k; t = s; break;
    }                           /* otherwise copy characters */
  }
  return n;                     /* return the number of characters */
}  /* isr_sinfo() */

/*--------------------------------------------------------------------*/

int isr_rinfo (ISREPORT *rep, SUPP_T supp, SUPP_T body, SUPP_T head,
               double eval)
{                               /* --- print ass. rule information */
  int        k, n = 0;          /* number of decimals, char. counter */
  double     smax;              /* maximum support */
  double     conf, lift;        /* buffers for computations */
  const char *s, *t;            /* to traverse the format */

  assert(rep);                  /* check the function arguments */
  if (!rep->format || !rep->file)
    return 0;                   /* check for a given format and file */
  smax = rep->supps[0];         /* get the total transaction weight */
  if (smax <= 0) smax = 1;      /* avoid divisions by zero */
  for (s = rep->format; *s; ) { /* traverse the output format */
    if (*s != '%') {            /* copy everything except '%' */
      isr_putc(rep, *s++); n++; continue; }
    t = s++; k = getsd(s, &s);  /* get the number of signif. digits */
    switch (*s++) {             /* evaluate the indicator character */
      case '%': isr_putc(rep, '%'); n++;                   break;
      case 'n': case 'd':
      #define double 0
      #define int    1
      #if SUPP_T==int
      case 'a': n += isr_intout(rep,      supp);           break;
      case 'b': n += isr_intout(rep,      body);           break;
      case 'h': n += isr_intout(rep,      head);           break;
      #else
      case 'a': n += isr_numout(rep,      supp,       k);  break;
      case 'b': n += isr_numout(rep,      body,       k);  break;
      case 'h': n += isr_numout(rep,      head,       k);  break;
      #endif
      #undef int
      #undef double
      case 's': n += isr_numout(rep,      supp/smax,  k);  break;
      case 'S': n += isr_numout(rep, 100*(supp/smax), k);  break;
      case 'x': n += isr_numout(rep,      body/smax,  k);  break;
      case 'X': n += isr_numout(rep, 100*(body/smax), k);  break;
      case 'y': n += isr_numout(rep,      head/smax,  k);  break;
      case 'Y': n += isr_numout(rep, 100*(head/smax), k);  break;
      case 'c': conf = (body > 0) ? supp/(double)body : 0;
                n += isr_numout(rep,      conf,       k);  break;
      case 'C': conf = (body > 0) ? supp/(double)body : 0;
                n += isr_numout(rep, 100* conf,       k);  break;
      case 'l': lift = ((body > 0) && (head > 0))
                     ? (supp*smax) /(body*(double)head) : 0;
                n += isr_numout(rep,      lift,       k);  break;
      case 'L': lift = ((body > 0) && (head > 0))
                     ? (supp*smax) /(body*(double)head) : 0;
                n += isr_numout(rep, 100* lift,       k);  break;
      case 'e': n += isr_numout(rep,      eval,       k);  break;
      case 'E': n += isr_numout(rep, 100* eval,       k);  break;
      case  0 : --s;            /* print the requested quantity */
      default : isr_putsn(rep, t, k = s-t); n += k; t = s; break;
    }                           /* otherwise copy characters */
  }
  return n;                     /* return the number of characters */
}  /* isr_rinfo() */

/*--------------------------------------------------------------------*/

void isr_getinfo (ISREPORT *rep, const char *sel, double *vals)
{                               /* --- get item set information */
  SUPP_T supp;                  /* support of current item set */
  double wgt;                   /* weight of current item set */
  double smax, wmax;            /* maximum support and weight */

  supp = rep->supps[rep->cnt];  /* get the set support and weight */
  wgt  = (rep->wgts) ? rep->wgts[rep->cnt] : 0;
  smax = rep->supps[0];         /* get maximum support and */
  if (smax <= 0) smax = 1;      /* avoid divisions by zero */
  wmax = (rep->wgts) ? rep->wgts[0] : smax;
  if (wmax <= 0) wmax = 1;      /* get maximum weight (if available) */
  for (; *sel; sel++, vals++) { /* traverse the information selectors */
    switch (*sel) {             /* and evaluate them */
      case 'i': *vals = (double)rep->cnt; break;
      case 'n': *vals = (double)supp;     break;
      case 'd': *vals = (double)supp;     break;
      case 'a': *vals = (double)supp;     break;
      case 's': *vals =      supp/smax;   break;
      case 'S': *vals = 100*(supp/smax);  break;
      case 'x': *vals =      supp/smax;   break;
      case 'X': *vals = 100*(supp/smax);  break;
      case 'w': *vals =      wgt;         break;
      case 'W': *vals = 100* wgt;         break;
      case 'r': *vals =      wgt /wmax;   break;
      case 'R': *vals = 100*(wgt /wmax);  break;
      case 'z': *vals = 100*(wgt *smax);  break;
      case 'e': *vals =      rep->eval;   break;
      case 'E': *vals = 100* rep->eval;   break;
      case 'p': *vals =      rep->eval;   break;
      case 'P': *vals = 100* rep->eval;   break;
      default : *vals =   0;              break;
    }                           /* store the corresponding value */
  }                             /* in the output vector */
}  /* isr_getinfo() */
