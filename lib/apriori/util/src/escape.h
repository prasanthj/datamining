/*----------------------------------------------------------------------
  File    : escape.h
  Contents: escape character handling
  Author  : Christian Borgelt
  History : 2010.08.22 file created from tabread.h
            2010.10.07 function esc_decode() changed to strtod() style
----------------------------------------------------------------------*/
#ifndef __ESCAPE__
#define __ESCAPE__

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
extern const char* esc_map[256];

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern const char* esc_encode (int c);
extern int         esc_decode (const char *s, char **end);

extern int         esc_enclen (const char *s);
extern int         esc_encstr (char *dst, const char *src);
extern int         esc_decstr (char *dst, const char *src);

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define esc_encode(c)   (esc_map[c])

#endif
