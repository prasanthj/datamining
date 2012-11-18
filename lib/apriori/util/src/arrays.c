/*----------------------------------------------------------------------
  File    : arrays.c
  Contents: some basic array operations, especially for pointer arrays
  Author  : Christian Borgelt
  History : 1996.09.16 file created as arrays.c
            1999.02.04 long int changed to int
            2001.06.03 function ptr_shuffle added
            2002.01.02 functions for basic data types added
            2002.03.03 functions ptr_reverse etc. added
            2003.08.21 function ptr_heapsort added
            2007.01.16 shuffle functions for basic data types added
            2007.12.02 bug in reverse functions fixed
            2008.08.01 renamed to arrays.c, some functions added
            2008.08.11 main function added (sortargs)
            2008.08.12 functions ptr_unique etc. added
            2008.08.17 binary search functions improved
            2008.10.05 functions to clear arrays added
            2010.07.31 index array sorting functions added
            2010.12.07 added several explicit type casts
            2011.09.28 function ptr_mrgsort() added (merge sort)
            2011.09.30 merge sort combined with insertion sort
            2012.06.03 functions for data type long int added
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "arrays.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define BUFSIZE     1024        /* size of fixed buffer for moving */
#define TH_INSERT   16          /* threshold for insertion sort */

/*----------------------------------------------------------------------
  Functions for Arrays of Basic Data Types
----------------------------------------------------------------------*/

#define MOVE(name,type) \
void name##_move (type *array, int off, int n, int pos) \
{                               /* --- move a number array section */  \
  int  i, end;                  /* loop variable, end index */         \
  type *src, *dst;              /* to traverse the array */            \
  type fxd[BUFSIZE], *buf;      /* buffer for copying */               \
                                                                       \
  assert(array                  /* check the function arguments */     \
     && (off >= 0) && (n >= 0) && (pos >= 0));                         \
  if ((pos >= off) && (pos <= off +n))                                 \
    return;                     /* check whether moving is necessary */\
  if (pos < off) { end = off +n; off = pos; pos = end -n; }            \
  else           { end = pos;               pos = off +n; }            \
  buf = fxd;                    /* normalize the indices */            \
  if (pos -off < end -pos) {    /* if first section is smaller */      \
    while (pos > off) {         /* while there are elements to shift */\
      n = pos -off;             /* get the number of elements */       \
      if (n > BUFSIZE) {        /* if the fixed buffer is too small */ \
        buf = (type*)malloc(n *sizeof(type));                          \
        if (!buf) { buf = fxd; n = BUFSIZE; }                          \
      }                         /* try to allocate a fitting buffer */ \
      src = array + pos -n;     /* get number of elements and */       \
      dst = buf;                /* copy source to the buffer */        \
      for (i = n;        --i >= 0; ) *dst++ = *src++;                  \
      dst = array +pos -n;      /* shift down/left second section */   \
      for (i = end -pos; --i >= 0; ) *dst++ = *src++;                  \
      src = buf;                /* copy buffer to destination */       \
      for (i = n;        --i >= 0; ) *dst++ = *src++;                  \
      pos -= n; end -= n;       /* second section has been shifted */  \
    } }                         /* down/left cnt elements */           \
  else {                        /* if second section is smaller */     \
    while (end > pos) {         /* while there are elements to shift */\
      n = end -pos;             /* get the number of elements */       \
      if (n > BUFSIZE) {        /* if the fixed buffer is too small */ \
        buf = (type*)malloc(n *sizeof(type));                          \
        if (!buf) { buf = fxd; n = BUFSIZE; }                          \
      }                         /* try to allocate a fitting buffer */ \
      src = array +pos +n;      /* get number of elements and */       \
      dst = buf +n;             /* copy source to the buffer */        \
      for (i = n;        --i >= 0; ) *--dst = *--src;                  \
      dst = array +pos +n;      /* shift up/right first section */     \
      for (i = pos -off; --i >= 0; ) *--dst = *--src;                  \
      src = buf +n;             /* copy buffer to destination */       \
      for (i = n;        --i >= 0; ) *--dst = *--src;                  \
      pos += n; off += n;       /* first section has been shifted */   \
    }                           /* up/right cnt elements */            \
  }                                                                    \
  if (buf != fxd) free(buf);    /* delete an allocated buffer */       \
}  /* move() */

/*--------------------------------------------------------------------*/

MOVE(sht, short)
MOVE(int, int)
MOVE(lng, long)
MOVE(flt, float)
MOVE(dbl, double)

/*--------------------------------------------------------------------*/

#define SELECT(name,type) \
void name##_select (type *array, int n, int k, RANDFN *rand) \
{                               /* --- shuffle array entries */        \
  int  i;                       /* array index */                      \
  type t;                       /* exchange buffer */                  \
                                                                       \
  assert(array && (n >= k) && (k >= 0));                               \
  if (k >= n) k = n-1;          /* adapt the number of selections */   \
  while (--k >= 0) {            /* shuffle loop (k selections) */      \
    i = (int)(rand() *n--);     /* compute a random index */           \
    if (i > n) i = n;           /* in the remaining section and */     \
    if (i < 0) i = 0;           /* exchange the array elements */      \
    t = array[i]; array[i] = array[0]; *array++ = t;                   \
  }                                                                    \
}  /* select() */

/*--------------------------------------------------------------------*/

SELECT(sht, short)
SELECT(int, int)
SELECT(lng, long)
SELECT(flt, float)
SELECT(dbl, double)

/*--------------------------------------------------------------------*/

#define SHUFFLE(name,type) \
void name##_shuffle (type *array, int n, RANDFN *rand) \
{ name##_select(array, n, n-1, rand); }

/*--------------------------------------------------------------------*/

SHUFFLE(sht, short)
SHUFFLE(int, int)
SHUFFLE(lng, long)
SHUFFLE(flt, float)
SHUFFLE(dbl, double)

/*--------------------------------------------------------------------*/

#define REVERSE(name, type) \
void name##_reverse (type *array, int n) \
{                               /* --- reverse a number array */       \
  type t;                       /* exchange buffer */                  \
  while (--n > 0) {             /* reverse the order of the elems. */  \
    t = array[n]; array[n--] = array[0]; *array++ = t; }               \
}  /* reverse */

/*--------------------------------------------------------------------*/

REVERSE(sht, short)
REVERSE(int, int)
REVERSE(lng, long)
REVERSE(flt, float)
REVERSE(dbl, double)

/*--------------------------------------------------------------------*/

#define QSORT(name,type) \
static void name##_qrec (type *array, int n) \
{                               /* --- recursive part of sort */       \
  type *l, *r;                  /* pointers to exchange positions */   \
  type x, t;                    /* pivot element and exchange buffer */\
  int  m;                       /* number of elements in sections */   \
                                                                       \
  do {                          /* sections sort loop */               \
    l = array; r = l +n -1;     /* start at left and right boundary */ \
    if (*l > *r) { t = *l; *l = *r; *r = t; }                          \
    x = array[n /2];            /* get the middle element as pivot */  \
    if      (x < *l) x = *l;    /* compute median of three */          \
    else if (x > *r) x = *r;    /* to find a better pivot */           \
    while (1) {                 /* split and exchange loop */          \
      while (*++l < x);         /* skip smaller elems. on the left */  \
      while (*--r > x);         /* skip greater elems. on the right */ \
      if (l >= r) {             /* if less than two elements left, */  \
        if (l <= r) { l++; r--; } break; }       /* abort the loop */  \
      t = *l; *l = *r; *r = t;  /* otherwise exchange elements */      \
    }                                                                  \
    m = array +n -l;            /* compute the number of elements */   \
    n = r -array +1;            /* right and left of the split */      \
    if (n > m) {                /* if right section is smaller, */     \
      if (m >= TH_INSERT)       /* but larger than the threshold, */   \
        name##_qrec(l, m); }    /* sort it by an recursive call */     \
    else {                      /* if the left section is smaller, */  \
      if (n >= TH_INSERT)       /* but larger than the threshold, */   \
        name##_qrec(array, n);  /* sort it by an recursive call, */    \
      array = l; n = m;         /* then switch to the right section */ \
    }                           /* keeping its size m in variable n */ \
  } while (n >= TH_INSERT);     /* while greater than threshold */     \
}  /* qrec() */                                                       \
                                                                       \
/*------------------------------------------------------------------*/ \
                                                                       \
void name##_qsort (type *array, int n)                                 \
{                               /* --- sort a number array */          \
  int  k;                       /* size of first section */            \
  type *l, *r;                  /* to traverse the array */            \
  type t;                       /* exchange buffer */                  \
                                                                       \
  assert(array && (n >= 0));    /* check the function arguments */     \
  if (n < 2) return;            /* do not sort less than two elems. */ \
  if (n < TH_INSERT)            /* if less elements than threshold */  \
    k = n;                      /* for insertion sort, note the */     \
  else {                        /* number of elements, otherwise */    \
    name##_qrec(array, n);      /* call the recursive sort function */ \
    k = TH_INSERT -1;           /* and get the number of elements */   \
  }                             /* in the first array section */       \
  for (l = r = array; --k > 0;) /* find position of smallest element */\
    if (*++r < *l) l = r;       /* within the first k elements */      \
  r = array;                    /* swap the smallest element */        \
  t = *l; *l = *r; *r = t;      /* to front as a sentinel */           \
  while (--n > 0) {             /* standard insertion sort */          \
    t = *++r;                   /* note the number to insert */        \
    for (l = r; *--l > t; k--)  /* shift right all numbers that are */ \
      l[1] = *l;                /* greater than the one to insert */   \
    l[1] = t;                   /* and store the number to insert */   \
  }                             /* in the place thus found */          \
}  /* qsort() */

/*--------------------------------------------------------------------*/

QSORT(sht, short)
QSORT(int, int)
QSORT(lng, long)
QSORT(flt, float)
QSORT(dbl, double)

/*--------------------------------------------------------------------*/

#define HEAPSORT(name,type) \
static void name##_sift (type *array, int l, int r)                    \
{                               /* --- let element sift down in heap */\
  type t;                       /* buffer for an array element */      \
  int  i;                       /* index of first successor in heap */ \
                                                                       \
  t = array[l];                 /* note the sift element */            \
  i = l +l +1;                  /* compute index of first successor */ \
  do {                          /* sift loop */                        \
    if ((i < r) && (array[i] < array[i+1]))                            \
      i++;                      /* if second successor is greater */   \
    if (t >= array[i])          /* if the successor is greater */      \
      break;                    /* than the sift element, */           \
    array[l] = array[i];        /* let the successor ascend in heap */ \
    l = i; i += i +1;           /* compute index of first successor */ \
  } while (i <= r);             /* while still within heap */          \
  array[l] = t;                 /* store the sift element */           \
}  /* sift() */                                                        \
                                                                       \
/*------------------------------------------------------------------*/ \
                                                                       \
void name##_heapsort (type *array, int n)                              \
{                               /* --- heap sort for number arrays */  \
  int  l, r;                    /* boundaries of heap section */       \
  type t;                       /* exchange buffer */                  \
                                                                       \
  assert(array && (n >= 0));    /* check the function arguments */     \
  if (n < 2) return;            /* do not sort less than two elems. */ \
  l = n /2;                     /* at start, only the second half */   \
  r = n -1;                     /* of the array has heap structure */  \
  while (--l >= 0)              /* while the heap is not complete, */  \
    name##_sift(array, l, r);   /* extend it by one element */         \
  while (1) {                   /* heap reduction loop */              \
    t = array[0];               /* swap the greatest element */        \
    array[0] = array[r];        /* to the end of the array */          \
    array[r] = t;                                                      \
    if (--r <= 0) break;        /* if the heap is empty, abort */      \
    name##_sift(array, 0, r);   /* let the element that has been */    \
  }                             /* swapped to front sift down */       \
}  /* heapsort() */

/*--------------------------------------------------------------------*/

HEAPSORT(sht, short)
HEAPSORT(int, int)
HEAPSORT(lng, long)
HEAPSORT(flt, float)
HEAPSORT(dbl, double)

/*--------------------------------------------------------------------*/

#define UNIQUE(name,type) \
int name##_unique (type *array, int n) \
{                               /* --- remove duplicate elements */    \
  type *s, *d;                  /* to traverse the array */            \
                                                                       \
  assert(array && (n >= 0));    /* check the function arguments */     \
  if (n <= 1) return n;         /* check for 0 or 1 element */         \
  for (d = s = array; --n > 0;) /* traverse the (sorted) array and */  \
    if (*++s != *d) *++d = *s;  /* collect the unique elements */      \
  return ++d -array;            /* return new number of elements */    \
}  /* unique() */

/*--------------------------------------------------------------------*/

UNIQUE(sht, short)
UNIQUE(int, int)
UNIQUE(lng, long)
UNIQUE(flt, float)
UNIQUE(dbl, double)

/*--------------------------------------------------------------------*/

#define BSEARCH(name,type) \
int name##_bsearch (type key, const type *array, int n) \
{                               /* --- do a binary search */           \
  int l, r, m;                  /* array indices */                    \
                                                                       \
  assert(array && (n >= 0));    /* check the function arguments */     \
  for (l = 0, r = n; l < r; ) { /* while search range is not empty */  \
    m = (l+r) /2;               /* compare the given key */            \
    if (key > array[m]) l = m+1;/* to the middle element */            \
    else                r = m;  /* adapt the search range */           \
  }                             /* according to the result */          \
  return ((l >= n) || (key != array[l])) ? -1-l : l;                   \
}  /* bsearch() */              /* return the (insertion) position */

/*--------------------------------------------------------------------*/

BSEARCH(sht, short)
BSEARCH(int, int)
BSEARCH(lng, long)
BSEARCH(flt, float)
BSEARCH(dbl, double)

/*----------------------------------------------------------------------
  Functions for Pointer Arrays
----------------------------------------------------------------------*/

void ptr_move (void *array, int off, int n, int pos)
{                               /* --- move a pointer array section */
  int  i, end;                  /* loop variable, end index */
  void **src, **dst;            /* to traverse the array */
  void *fxd[BUFSIZE], **buf;    /* buffer for copying */

  assert(array                  /* check the function arguments */
     && (off >= 0) && (n >= 0) && (pos >= 0));
  if ((pos >= off) && (pos <= off +n))
    return;                     /* check whether moving is necessary */
  if (pos < off) { end = off +n; off = pos; pos = end -n; }
  else           { end = pos;               pos = off +n; }
  buf = fxd;                    /* normalize the indices */
  if (pos -off < end -pos) {    /* if first section is smaller */
    while (pos > off) {         /* while there are elements to shift */
      n = pos -off;             /* get the number of elements */
      if (n > BUFSIZE) {        /* if the fixed buffer is too small */
        buf = (void**)malloc(n *sizeof(void*));
        if (!buf) { buf = fxd; n = BUFSIZE; }
      }                         /* try to allocate a fitting buffer */
      src = (void**)array+pos-n;/* get number of elements and */
      dst = buf;                /* copy source to the buffer */
      for (i = n;        --i >= 0; ) *dst++ = *src++;
      dst = (void**)array+pos-n;/* shift down/left second section */
      for (i = end -pos; --i >= 0; ) *dst++ = *src++;
      src = buf;                /* copy buffer to destination */
      for (i = n;        --i >= 0; ) *dst++ = *src++;
      pos -= n; end -= n;       /* second section has been shifted */
    } }                         /* down/left cnt elements */
  else {                        /* if second section is smaller */
    while (end > pos) {         /* while there are elements to shift */
      n = end -pos;             /* get the number of elements */
      if (n > BUFSIZE) {        /* if the fixed buffer is too small */
        buf = (void**)malloc(n *sizeof(void*));
        if (!buf) { buf = fxd; n = BUFSIZE; }
      }                         /* try to allocate a fitting buffer */
      src = (void**)array+pos+n;/* get number of elements and */
      dst = buf +n;             /* copy source to the buffer */
      for (i = n;        --i >= 0; ) *--dst = *--src;
      dst = (void**)array+pos+n;/* shift up/right first section */
      for (i = pos -off; --i >= 0; ) *--dst = *--src;
      src = buf +n;             /* copy buffer to destination */
      for (i = n;        --i >= 0; ) *--dst = *--src;
      pos += n; off += n;       /* first section has been shifted */
    }                           /* up/right cnt elements */
  }
  if (buf != fxd) free(buf);    /* delete an allocated buffer */
}  /* ptr_move() */

/*--------------------------------------------------------------------*/

void ptr_select (void *array, int n, int k, RANDFN *rand)
{                               /* --- select random array entries */
  int  i;                       /* array index */
  void **a = (void**)array;     /* array to sort */
  void *t;                      /* exchange buffer */

  assert(array                  /* check the function arguments */
      && rand && (n >= k) && (k >= 0));
  if (k >= n) k = n-1;          /* adapt the number of selections */
  while (--k >= 0) {            /* shuffle loop (k selections) */
    i = (int)(rand() *n--);     /* compute a random index */
    if (i > n) i = n;           /* in the remaining section and */
    if (i < 0) i = 0;           /* clamp it to a valid range */
    t = a[i]; a[i] = a[0]; *a++ = t;
  }                             /* exchange the array elements */
}  /* ptr_select() */

/*--------------------------------------------------------------------*/

void ptr_shuffle (void *array, int n, RANDFN *rand)
{ ptr_select(array, n, n-1, rand); }

/*--------------------------------------------------------------------*/

void ptr_reverse (void *array, int n)
{                               /* --- reverse a pointer array */
  void **a = (void**)array;     /* array to sort */
  void *t;                      /* exchange buffer */

  assert(array && (n >= 0));    /* check the function arguments */
  while (--n > 0) {             /* reverse the order of the elements */
    t = a[n]; a[n--] = a[0]; *a++ = t; }
}  /* ptr_reverse() */

/*--------------------------------------------------------------------*/

static void qrec (void **array, int n, CMPFN *cmp, void *data)
{                               /* --- recursive part of quicksort */
  void **l, **r;                /* pointers to exchange positions */
  void *x,  *t;                 /* pivot element and exchange buffer */
  int  m;                       /* number of elements in 2nd section */

  do {                          /* sections sort loop */
    l = array; r = l +n -1;     /* start at left and right boundary */
    if (cmp(*l, *r, data) > 0){ /* bring the first and last */
      t = *l; *l = *r; *r = t;} /* element into proper order */
    x = array[n /2];            /* get the middle element as pivot */
    if      (cmp(x, *l, data) < 0) x = *l;  /* try to find a */
    else if (cmp(x, *r, data) > 0) x = *r;  /* better pivot */
    while (1) {                 /* split and exchange loop */
      while (cmp(*++l, x, data) < 0)      /* skip left  elements that */
        ;                       /* are smaller than the pivot element */
      while (cmp(*--r, x, data) > 0)      /* skip right elements that */
        ;                       /* are greater than the pivot element */
      if (l >= r) {             /* if less than two elements left, */
        if (l <= r) { l++; r--; } break; }       /* abort the loop */
      t = *l; *l = *r; *r = t;  /* otherwise exchange elements */
    }
    m = array +n -l;            /* compute the number of elements */
    n = r -array +1;            /* right and left of the split */
    if (n > m) {                /* if right section is smaller, */
      if (m >= TH_INSERT)       /* but larger than the threshold, */
        qrec(l, m, cmp, data); }/* sort it by a recursive call, */
    else {                      /* if the left section is smaller, */
      if (n >= TH_INSERT)       /* but larger than the threshold, */
        qrec(array,n,cmp,data); /* sort it by a recursive call, */
      array = l; n = m;         /* then switch to the right section */
    }                           /* keeping its size m in variable n */
  } while (n >= TH_INSERT);     /* while greater than threshold */
}  /* qrec() */

/*--------------------------------------------------------------------*/

void ptr_qsort (void *array, int n, CMPFN *cmp, void *data)
{                               /* --- quicksort for pointer arrays */
  int  k;                       /* size of first section */
  void **l, **r;                /* to traverse the array */
  void *t;                      /* exchange buffer */

  assert(array && (n >= 0) && cmp); /* check the function arguments */
  if (n <= 1) return;           /* do not sort less than two elements */
  if (n < TH_INSERT)            /* if fewer elements than threshold */
    k = n;                      /* for insertion sort, note the */
  else {                        /* number of elements, otherwise */
    qrec((void**)array, n, cmp, data);
    k = TH_INSERT -1;           /* call the recursive function and */
  }                             /* get size of first array section */
  for (l = r = (void**)array; --k > 0; )
    if (cmp(*++r, *l, data) < 0)
      l = r;                    /* find smallest of first k elements */
  r = (void**)array;            /* swap the smallest element */
  t = *l; *l = *r; *r = t;      /* to the front as a sentinel */
  while (--n > 0) {             /* insertion sort loop */
    t = *++r;                   /* note the element to insert */
    for (l = r; cmp(*--l, t, data) > 0; )   /* shift right elements */
      l[1] = *l;                /* that are greater than the one to */
    l[1] = t;                   /* insert and store the element to */
  }                             /* insert in the place thus found */
}  /* ptr_qsort() */

/*--------------------------------------------------------------------*/

static void sift (void **array, int l, int r, CMPFN *cmp, void *data)
{                               /* --- let element sift down in heap */
  void *t;                      /* buffer for an array element */
  int  i;                       /* index of first successor in heap */

  t = array[l];                 /* note the sift element */
  i = l +l +1;                  /* compute index of first successor */
  do {                          /* sift loop */
    if ((i < r)                 /* if second successor is greater */
    &&  (cmp(array[i], array[i+1], data) < 0))
      i++;                      /* go to the second successor */
    if (cmp(t, array[i], data) >= 0) /* if the successor is greater */
      break;                         /* than the sift element, */
    array[l] = array[i];        /* let the successor ascend in heap */
    l = i; i += i +1;           /* compute index of first successor */
  } while (i <= r);             /* while still within heap */
  array[l] = t;                 /* store the sift element */
}  /* sift() */

/*--------------------------------------------------------------------*/

void ptr_heapsort (void *array, int n, CMPFN *cmp, void *data)
{                               /* --- heap sort for pointer arrays */
  int  l, r;                    /* boundaries of heap section */
  void *t, **a;                 /* exchange buffer, array */

  assert(array && (n >= 0) && cmp); /* check the function arguments */
  if (n <= 1) return;           /* do not sort less than two elements */
  l = n /2;                     /* at start, only the second half */
  r = n -1;                     /* of the array has heap structure */
  a = (void**)array;            /* type the array pointer */
  while (--l >= 0)              /* while the heap is not complete, */
    sift(a, l, r, cmp, data);   /* extend it by one element */
  while (1) {                   /* heap reduction loop */
    t = a[0]; a[0] = a[r];      /* swap the greatest element */
    a[r] = t;                   /* to the end of the array */
    if (--r <= 0) break;        /* if the heap is empty, abort */
    sift(a, 0, r, cmp, data);   /* let the element that has been */
  }                             /* swapped to front sift down */
}  /* ptr_heapsort() */

/*--------------------------------------------------------------------*/

static void mrgsort (void **array, void **buf, int n,
                     CMPFN *cmp, void *data)
{                               /* --- merge sort for pointer arrays */
  int  k, a, b;                 /* numbers of objects in sections */
  void **sa, **sb, **ea, **eb;  /* starts and ends of sorted sections */
  void **d, *t;                 /* merge destination, exchange buffer */

  assert(array && buf && (n > 0) && cmp);
  if (n <= 8) {                 /* if only few elements to sort */
    for (sa = array; --n > 0;){ /* insertion sort loop */
      t = *(d = ++sa);          /* note the element to insert */
      while ((--d >= array)     /* while not at the array start, */
      &&     (cmp(*d, t, data) > 0))     /* shift right elements */
        d[1] = *d;              /* that are greater than the one */
      d[1] = t;                 /* to insert and store the element */
    } return;                   /* to insert in the place thus found */
  }                             /* aftwards sorting is done, so abort */
  /* Using insertion sort for less than eight elements is not only */
  /* slightly faster, but also ensures that all subsections sorted */
  /* recursively in the code below contain at least two elements.  */

  k = n/2; d = buf;             /* sort two subsections recursively */
  mrgsort(sa = array,   d,   a = k/2, cmp, data);
  mrgsort(sb = sa+a,    d+a, b = k-a, cmp, data);
  for (ea = sb, eb = sb+b; 1;){ /* traverse the sorted sections */
    if (cmp(*sa, *sb, data) <= 0)
         { *d++ = *sa++; if (sa >= ea) break; }
    else { *d++ = *sb++; if (sb >= eb) break; }
  }                             /* copy smaller element to dest. */
  while (sa < ea) *d++ = *sa++; /* copy remaining elements */
  while (sb < eb) *d++ = *sb++; /* from source to destination */

  n -= k; d = buf+k;            /* sort two subsections recursively */
  mrgsort(sa = array+k, d,   a = n/2, cmp, data);
  mrgsort(sb = sa+a,    d+a, b = n-a, cmp, data);
  for (ea = sb, eb = sb+b; 1;){ /* traverse the sorted sections */
    if (cmp(*sa, *sb, data) <= 0)
         { *d++ = *sa++; if (sa >= ea) break; }
    else { *d++ = *sb++; if (sb >= eb) break; }
  }                             /* copy smaller element to dest. */
  while (sa < ea) *d++ = *sa++; /* copy remaining elements */
  while (sb < eb) *d++ = *sb++; /* from source to destination */

  sa = buf; sb = sa+k; d = array;
  for (ea = sb, eb = sb+n; 1;){ /* traverse the sorted sections */
    if (cmp(*sa, *sb, data) <= 0)
         { *d++ = *sa++; if (sa >= ea) break; }
    else { *d++ = *sb++; if (sb >= eb) break; }
  }                             /* copy smaller element to dest. */
  while (sa < ea) *d++ = *sa++; /* copy remaining elements */
  while (sb < eb) *d++ = *sb++; /* from source to destination */
}  /* mrgsort() */

/*--------------------------------------------------------------------*/

int ptr_mrgsort (void *array, int n, CMPFN *cmp, void *data, void *buf)
{                               /* --- merge sort for pointer arrays */
  void **b;                     /* (allocated) buffer */

  assert(array && (n >= 0) && cmp);  /* check the function arguments */
  if (n < 2) return 0;          /* do not sort less than two objects */
  if (!(b = (void**)buf) && !(b = (void**)malloc(n *sizeof(void*))))
    return -1;                  /* allocate a buffer if not given */
  mrgsort(array, buf, n, cmp, data);
  if (!buf) free(b);            /* sort the array recursively */
  return 0;                     /* return 'ok' */
}  /* ptr_mrgsort() */

/* This implementation of merge sort is stable, that is, it does not  */
/* change the relative order of elements that are considered equal by */
/* the comparison function. Thus it maintains the order of a previous */
/* sort with another comparison function as long as the order imposed */
/* by the current comparison function does not override this order.   */

/*--------------------------------------------------------------------*/

int ptr_bsearch (const void *key,
                 const void *array, int n, CMPFN *cmp, void *data)
{                               /* --- do a binary search */
  int  l, r, m, c, x = -1;      /* array indices, comparison result */
  void **a;                     /* typed array */

  assert(key && array && (n >= 0) && cmp); /* check arguments */
  a = (void**)array;            /* type the array pointer */
  for (l = 0, r = n; l < r; ) { /* while search range is not empty */
    m = (l+r) /2;               /* compare the given key */
    c = cmp(key, a[m], data);   /* to the middle element */
    if (c > 0)  l = m+1;        /* adapt the search range */
    else x = c, r = m;          /* according to the result */
  }
  return (x) ? -1-l : l;        /* return the (insertion) position */
}  /* ptr_bsearch() */

/*--------------------------------------------------------------------*/

int ptr_unique (void *array, int n, CMPFN *cmp, void *data, OBJFN *del)
{                               /* --- remove duplicate elements */
  void **s, **d;                /* to traverse the pointer array */

  assert(array && (n >= 0) && cmp);  /* check the function arguments */
  if (n <= 1) return n;         /* check for 0 or 1 element */
  for (d = s = (void**)array; --n > 0; ) {
    if (cmp(*++s, *d, data) != 0) *++d = *s;
    else if (del) del(*s);      /* traverse the (sorted) array */
  }                             /* and collect unique elements */
  return ++d -(void**)array;    /* return the new number of elements */
}  /* ptr_unique() */

/*----------------------------------------------------------------------
  Functions for Index Arrays
----------------------------------------------------------------------*/

#define IDX_QSORT(name,type) \
static void name##_qrec (int *index, int n, const type *array)         \
{                               /* --- recursive part of sort */       \
  int  *l, *r;                  /* pointers to exchange positions */   \
  int  x, t;                    /* pivot element and exchange buffer */\
  int  m;                       /* number of elements in sections */   \
  type p, a, z;                 /* buffers for array elements */       \
                                                                       \
  do {                          /* sections sort loop */               \
    l = index; r = l +n -1;     /* start at left and right boundary */ \
    a = array[*l];              /* get the first and last elements */  \
    z = array[*r];              /* and bring them into right order */  \
    if (a > z) { t = *l; *l = *r; *r = t; }                            \
    x = index[n /2];            /* get the middle element as pivot */  \
    p = array[x];               /* and array element referred to */    \
    if      (p < a) { p = a; x = *l; }  /* compute median of three */  \
    else if (p > z) { p = z; x = *r; }  /* to find a better pivot */   \
    while (1) {                 /* split and exchange loop */          \
      while (array[*++l] < p);  /* skip smaller elems. on the left */  \
      while (array[*--r] > p);  /* skip greater elems. on the right */ \
      if (l >= r) {             /* if less than two elements left, */  \
        if (l <= r) { l++; r--; } break; }       /* abort the loop */  \
      t = *l; *l = *r; *r = t;  /* otherwise exchange elements */      \
    }                                                                  \
    m = index +n -l;            /* compute the number of elements */   \
    n = r -index +1;            /* right and left of the split */      \
    if (n > m) {                /* if right section is smaller, */     \
      if (m >= TH_INSERT)       /* but larger than the threshold, */   \
        name##_qrec(l,     m, array); }   /* sort it recursively, */   \
    else {                      /* if the left section is smaller, */  \
      if (n >= TH_INSERT)       /* but larger than the threshold, */   \
        name##_qrec(index, n, array);     /* sort it recursively, */   \
      index = l; n = m;         /* then switch to the right section */ \
    }                           /* keeping its size m in variable n */ \
  } while (n >= TH_INSERT);     /* while greater than threshold */     \
}  /* qrec() */                                                       \
                                                                       \
/*------------------------------------------------------------------*/ \
                                                                       \
void name##_qsort (int *index, int n, const type *array)               \
{                               /* --- sort a number array */          \
  int  k;                       /* size of first section */            \
  int  *l, *r;                  /* to traverse the array */            \
  int  x;                       /* exchange buffer */                  \
  type t;                       /* buffer for element referred to */   \
                                                                       \
  assert(index && (n >= 0) && array); /* check function arguments */   \
  if (n <= 1) return;           /* do not sort less than two elems. */ \
  if (n < TH_INSERT)            /* if less elements than threshold */  \
    k = n;                      /* for insertion sort, note the */     \
  else {                        /* number of elements, otherwise */    \
    name##_qrec(index, n, array);     /* call recursive function */    \
    k = TH_INSERT -1;           /* and get the number of elements */   \
  }                             /* in the first array section */       \
  for (l = r = index; --k > 0;) /* find the position */                \
    if (array[*++r] < array[*l])/* of the smallest element */          \
      l = r;                    /* within the first k elements */      \
  r = index;                    /* swap the smallest element */        \
  x = *l; *l = *r; *r = x;      /* to front as a sentinel */           \
  while (--n > 0) {             /* standard insertion sort */          \
    t = array[x = *++r];        /* note the number to insert */        \
    for (l = r; array[*--l] > t; k--) /* shift right all that are */   \
      l[1] = *l;                /* greater than the one to insert */   \
    l[1] = x;                   /* and store the number to insert */   \
  }                             /* in the place thus found */          \
}  /* qsort() */

/*--------------------------------------------------------------------*/

IDX_QSORT(i2s, short)
IDX_QSORT(i2i, int)
IDX_QSORT(i2l, long)
IDX_QSORT(i2f, float)
IDX_QSORT(i2d, double)

/*--------------------------------------------------------------------*/

#define IDX_HEAPSORT(name,type) \
static void name##_sift (int *index, int l, int r,                     \
                            const type *array)                         \
{                               /* --- let element sift down in heap */\
  int  i;                       /* index of first successor in heap */ \
  int  x;                       /* buffer for an index element */      \
  type t;                       /* buffer for an array element */      \
                                                                       \
  t = array[x = index[l]];      /* note the sift element */            \
  i = l +l +1;                  /* compute index of first successor */ \
  do {                          /* sift loop */                        \
    if ((i < r) && (array[index[i]] < array[index[i+1]]))              \
      i++;                      /* if second successor is greater */   \
    if (t >= array[index[i]])   /* if the successor is greater */      \
      break;                    /* than the sift element, */           \
    index[l] = index[i];        /* let the successor ascend in heap */ \
    l = i; i += i +1;           /* compute index of first successor */ \
  } while (i <= r);             /* while still within heap */          \
  index[l] = x;                 /* store the sift element */           \
}  /* sift() */                                                       \
                                                                       \
/*------------------------------------------------------------------*/ \
                                                                       \
void name##_heapsort (int *index, int n, const type *array)            \
{                               /* --- heap sort for number arrays */  \
  int l, r;                     /* boundaries of heap section */       \
  int t;                        /* exchange buffer */                  \
                                                                       \
  assert(index && (n >= 0) && array);   /* check function arguments */ \
  if (n <= 1) return;           /* do not sort less than 2 elements */ \
  l = n /2;                     /* at start, only the second half */   \
  r = n -1;                     /* of the array has heap structure */  \
  while (--l >= 0)              /* while the heap is not complete, */  \
    name##_sift(index, l, r, array);   /* extend it by one element */  \
  while (1) {                   /* heap reduction loop */              \
    t        = index[0];        /* swap the greatest element */        \
    index[0] = index[r];        /* to the end of the array */          \
    index[r] = t;                                                      \
    if (--r <= 0) break;        /* if the heap is empty, abort */      \
    name##_sift(index, 0, r, array);                                   \
  }                             /* let the swap element sift down */   \
}  /* heapsort() */

/*--------------------------------------------------------------------*/

IDX_HEAPSORT(i2s, short)
IDX_HEAPSORT(i2i, int)
IDX_HEAPSORT(i2l, long)
IDX_HEAPSORT(i2f, float)
IDX_HEAPSORT(i2d, double)

/*--------------------------------------------------------------------*/

static void i2p_qrec (int *index, int n, const void **array,
                      CMPFN *cmp, void *data)
{                               /* --- recursive part of quicksort */
  int *l, *r;                   /* pointers to exchange positions */
  int x,  t;                    /* pivot element and exchange buffer */
  int m;                        /* number of elements in 2nd section */
  const void *p, *a, *z;        /* buffers for array elements */

  do {                          /* sections sort loop */
    l = index; r = l +n -1;     /* start at left and right boundary */
    a = array[*l];              /* get the first and last elements */
    z = array[*r];              /* and bring them into right order */
    if (cmp(a, z, data) > 0) { t = *l; *l = *r; *r = t; }
    x = index[n /2];            /* get the middle element as pivot */
    p = array[x];               /* compute median of three to improve */
    if      (cmp(p, a, data) < 0) { p = a; x = *l; }
    else if (cmp(p, z, data) > 0) { p = z; x = *r; }
    while (1) {                 /* split and exchange loop */
      while (cmp(array[*++l], p, data) < 0)
        ;                       /* skip elements smaller than pivot */
      while (cmp(array[*--r], p, data) > 0)
        ;                       /* skip elements greater than pivot */
      if (l >= r) {             /* if less than two elements left, */
        if (l <= r) { l++; r--; } break; }       /* abort the loop */
      t = *l; *l = *r; *r = t;  /* otherwise exchange elements */
    }
    m = index +n -l;            /* compute the number of elements */
    n = r -index +1;            /* right and left of the split */
    if (n > m) {                /* if right section is smaller, */
      if (m >= TH_INSERT)       /* but larger than the threshold, */
        i2p_qrec(l,     m, array, cmp, data); }       /* sort it, */
    else {                      /* if the left section is smaller, */
      if (n >= TH_INSERT)       /* but larger than the threshold, */
        i2p_qrec(index, n, array, cmp, data);         /* sort it, */
      index = l; n = m;         /* then switch to the right section */
    }                           /* keeping its size m in variable n */
  } while (n >= TH_INSERT);     /* while greater than threshold */
}  /* i2p_qrec() */

/*--------------------------------------------------------------------*/

void i2p_qsort (int *index, int n, const void **array,
                CMPFN *cmp, void *data)
{                               /* --- quick sort for index arrays */
  int k;                        /* size of first section */
  int *l, *r;                   /* to traverse the array */
  int x;                        /* exchange buffer */
  const void *t;                /* buffer for pointer referred to */

  assert(index                  /* check the function arguments */
  &&    (n >= 0) && array && cmp);
  if (n <= 1) return;           /* do not sort less than two elements */
  if (n < TH_INSERT)            /* if fewer elements than threshold */
    k = n;                      /* for insertion sort, note the */
  else {                        /* number of elements, otherwise */
    i2p_qrec(index, n, array, cmp, data);    /* sort recursively */
    k = TH_INSERT -1;           /* and get the number of elements */
  }                             /* in the first array section */
  for (l = r = index; --k > 0;) /* find the smallest element */
    if (cmp(array[*++r], array[*l], data) < 0)
      l = r;                    /* among the first k elements */
  r = index;                    /* swap the smallest element */
  x = *l; *l = *r; *r = x;      /* to the front as a sentinel */
  while (--n > 0) {             /* insertion sort loop */
    t = array[x = *++r];        /* note the element to insert */
    for (l = r; cmp(array[*--l], t, data) > 0; )
      l[1] = *l;                /* shift right index elements */
    l[1] = x;                   /* that are greater than the one to */
  }                             /* insert and store the element to */
}  /* i2p_qsort() */            /* insert in the place thus found */

/*--------------------------------------------------------------------*/

static void i2p_sift (int *index, int l, int r, const void **array,
                      CMPFN *cmp, void *data)
{                               /* --- let element sift down in heap */
  int i;                        /* index of first successor in heap */
  int x;                        /* buffer for an index element */
  const void *t;                /* buffer for an array element */

  t = array[x = index[l]];      /* note the sift element */
  i = l +l +1;                  /* compute index of first successor */
  do {                          /* sift loop */
    if ((i < r)                 /* if second successor is greater */
    &&  (cmp(array[index[i]], array[index[i+1]], data) < 0))
      i++;                      /* go to the second successor */
    if (cmp(t, array[i], data) >= 0) /* if the successor is greater */
      break;                         /* than the sift element, */
    index[l] = index[i];        /* let the successor ascend in heap */
    l = i; i += i +1;           /* compute index of first successor */
  } while (i <= r);             /* while still within heap */
  index[l] = x;                 /* store the sift element */
}  /* i2p_sift() */

/*--------------------------------------------------------------------*/

void i2p_heapsort (int *index, int n, const void **array,
                   CMPFN *cmp, void *data)
{                               /* --- heap sort for index arrays */
  int l, r;                     /* boundaries of heap section */
  int t;                        /* exchange buffer */

  assert(index                  /* check the function arguments */
  &&    (n >= 0) && array && cmp);
  if (n <= 1) return;           /* do not sort less than two elements */
  l = n /2;                     /* at start, only the second half */
  r = n -1;                     /* of the array has heap structure */
  while (--l >= 0)              /* while the heap is not complete, */
    i2p_sift(index, l, r, array, cmp, data);          /* extend it */
  while (1) {                   /* heap reduction loop */
    t        = index[0];        /* swap the greatest element */
    index[0] = index[r];        /* to the end of the array */
    index[r] = t;
    if (--r <= 0) break;        /* if the heap is empty, abort */
    i2p_sift(index, 0, r, array, cmp, data);
  }                             /* let the element that has been */
}  /* i2p_heapsort() */         /* swapped to front sift down */

/*--------------------------------------------------------------------*/

static void i2x_qrec (int *index, int n, ICMPFN *cmp, void *data)
{                               /* --- recursive part of quicksort */
  int *l, *r;                   /* pointers to exchange positions */
  int x,  t;                    /* pivot element and exchange buffer */
  int m;                        /* number of elements in 2nd section */

  do {                          /* sections sort loop */
    l = index; r = l +n -1;     /* start at left and right boundary */
    if (cmp(*l, *r, data) > 0){ /* bring the first and last */
      t = *l; *l = *r; *r = t;} /* element into proper order */
    x = index[n /2];            /* get the middle element as pivot */
    if      (cmp(x, *l, data) < 0) x = *l;  /* try to find a */
    else if (cmp(x, *r, data) > 0) x = *r;  /* better pivot */
    while (1) {                 /* split and exchange loop */
      while (cmp(*++l, x, data) < 0)      /* skip left  elements that */
        ;                       /* are smaller than the pivot element */
      while (cmp(*--r, x, data) > 0)      /* skip right elements that */
        ;                       /* are greater than the pivot element */
      if (l >= r) {             /* if less than two elements left, */
        if (l <= r) { l++; r--; } break; }       /* abort the loop */
      t = *l; *l = *r; *r = t;  /* otherwise exchange elements */
    }
    m = index +n -l;            /* compute the number of elements */
    n = r -index +1;            /* right and left of the split */
    if (n > m) {                /* if right section is smaller, */
      if (m >= TH_INSERT)       /* but larger than the threshold, */
        i2x_qrec(l,     m, cmp, data);}   /* sort it recursively, */
    else {                      /* if the left section is smaller, */
      if (n >= TH_INSERT)       /* but larger than the threshold, */
        i2x_qrec(index, n, cmp, data);    /* sort it recursively, */
      index = l; n = m;         /* then switch to the right section */
    }                           /* keeping its size m in variable n */
  } while (n >= TH_INSERT);     /* while greater than threshold */
}  /* i2x_qrec() */

/*--------------------------------------------------------------------*/

void i2x_qsort (int *index, int n, ICMPFN *cmp, void *data)
{                               /* --- quicksort for index arrays */
  int k;                        /* size of first section */
  int *l, *r;                   /* to traverse the array */
  int t;                        /* exchange buffer */

  assert(index && (n >= 0) && cmp); /* check the function arguments */
  if (n <= 1) return;           /* do not sort less than two elements */
  if (n < TH_INSERT)            /* if fewer elements than threshold */
    k = n;                      /* for insertion sort, note the */
  else {                        /* number of elements, otherwise */
    i2x_qrec(index, n, cmp, data);    /* call recursice function */
    k = TH_INSERT -1;           /* and get the number of elements */
  }                             /* in the first array section */
  for (l = r = index; --k > 0;) /* find the smallest element within */
    if (cmp(*++r, *l, data) < 0) l = r;     /* the first k elements */
  r = index;                    /* swap the smallest element */
  t = *l; *l = *r; *r = t;      /* to the front as a sentinel */
  while (--n > 0) {             /* insertion sort loop */
    t = *++r;                   /* note the element to insert */
    for (l = r; cmp(*--l, t, data) > 0; )   /* shift right elements */
      l[1] = *l;                /* that are greater than the one to */
    l[1] = t;                   /* insert and store the element to */
  }                             /* insert in the place thus found */
}  /* i2x_qsort() */

/*--------------------------------------------------------------------*/

static void i2x_sift (int *index, int l, int r, ICMPFN *cmp, void *data)
{                               /* --- let element sift down in heap */
  int i;                        /* index of first successor in heap */
  int t;                        /* buffer for an array element */

  t = index[l];                 /* note the sift element */
  i = l +l +1;                  /* compute index of first successor */
  do {                          /* sift loop */
    if ((i < r)                 /* if second successor is greater */
    &&  (cmp(index[i], index[i+1], data) < 0))
      i++;                      /* go to the second successor */
    if (cmp(t, index[i], data) >= 0) /* if the successor is greater */
      break;                         /* than the sift element, */
    index[l] = index[i];        /* let the successor ascend in heap */
    l = i; i += i +1;           /* compute index of first successor */
  } while (i <= r);             /* while still within heap */
  index[l] = t;                 /* store the sift element */
}  /* sift() */

/*--------------------------------------------------------------------*/

void i2x_heapsort (int *index, int n, ICMPFN *cmp, void *data)
{                               /* --- heap sort for index arrays */
  int l, r;                     /* boundaries of heap section */
  int t;                        /* exchange buffer */

  assert(index && (n >= 0) && cmp); /* check the function arguments */
  if (n <= 1) return;           /* do not sort less than two elements */
  l = n /2;                     /* at start, only the second half */
  r = n -1;                     /* of the index has heap structure */
  while (--l >= 0)              /* while the heap is not complete, */
    i2x_sift(index, l, r, cmp, data);  /* extend it by one element */
  while (1) {                   /* heap reduction loop */
    t        = index[0];        /* swap the greatest element */
    index[0] = index[r];        /* to the end of the index */
    index[r] = t;
    if (--r <= 0) break;        /* if the heap is empty, abort */
    i2x_sift(index, 0, r, cmp, data);
  }                             /* let the element that has been */
}  /* i2x_heapsort() */         /* swapped to front sift down */

/*----------------------------------------------------------------------
  Main Function for Testing
----------------------------------------------------------------------*/
#ifdef ARRAYS_MAIN

static int lexcmp (const void *p1, const void *p2, void *data)
{                               /* --- compare lexicographically */
  return strcmp(p1, p2);        /* use standard string comparison */
}  /* lexcmp() */

/*--------------------------------------------------------------------*/

static int numcmp (const void *p1, const void *p2, void *data)
{                               /* --- compare strings numerically */
  double d1 = strtod((const char*)p1, NULL);
  double d2 = strtod((const char*)p2, NULL);
  if (d1 < d2) return -1;       /* convert to numbers and */
  if (d1 > d2) return +1;       /* compare numerically */
  return strcmp(p1, p2);        /* if the numbers are equal, */
}  /* numcmp() */               /* compare strings lexicographically */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- sort program arguments */
  int   i, n;                   /* loop variables */
  char  *s;                     /* to traverse the arguments */
  CMPFN *cmp = lexcmp;          /* comparison function */

  if (argc < 2) {               /* if no arguments are given */
    printf("usage: %s [arg [arg ...]]\n", argv[0]);
    printf("sort the list of program arguments\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */
  for (i = n = 0; ++i < argc; ) {
    s = argv[i];                /* traverse the arguments */
    if (*s != '-') { argv[n++] = s; continue; }
    s++;                        /* store the arguments to sort */
    while (*s) {                /* traverse the options */
      switch (*s++) {           /* evaluate the options */
        case 'n': cmp = numcmp; break;
        default : printf("unknown option -%c\n", *--s); return -1;
      }                         /* set the option variables */
    }                           /* and check for known options */
  }
  ptr_qsort(argv, n, cmp,NULL); /* sort the program arguments */
  for (i = 0; i < n; i++) {     /* print the sorted arguments */
    fputs(argv[i], stdout); fputc('\n', stdout); }
  return 0;                     /* return 'ok' */
}  /* main() */

#endif
