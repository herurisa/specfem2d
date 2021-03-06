/* Copyright 2004,2007,2008,2010 ENSEIRB, INRIA & CNRS
**
** This file is part of the Scotch software package for static mapping,
** graph partitioning and sparse matrix ordering.
**
** This software is governed by the CeCILL-C license under French law
** and abiding by the rules of distribution of free software. You can
** use, modify and/or redistribute the software under the terms of the
** CeCILL-C license as circulated by CEA, CNRS and INRIA at the following
** URL: "http://www.cecill.info".
**
** As a counterpart to the access to the source code and rights to copy,
** modify and redistribute granted by the license, users are provided
** only with a limited warranty and the software's author, the holder of
** the economic rights, and the successive licensors have only limited
** liability.
**
** In this respect, the user's attention is drawn to the risks associated
** with loading, using, modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean that it is complicated to manipulate, and that also
** therefore means that it is reserved for developers and experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards
** their requirements in conditions enabling the security of their
** systems and/or data to be ensured and, more generally, to use and
** operate it in the same conditions as regards security.
**
** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
*/
/************************************************************/
/**                                                        **/
/**   NAME       : common_memory.c                         **/
/**                                                        **/
/**   AUTHORS    : Francois PELLEGRINI                     **/
/**                                                        **/
/**   FUNCTION   : Part of a parallel direct block solver. **/
/**                This module handles errors.             **/
/**                                                        **/
/**   DATES      : # Version 0.0  : from : 07 sep 2001     **/
/**                                 to     07 sep 2001     **/
/**                # Version 0.1  : from : 14 apr 2001     **/
/**                                 to     24 mar 2003     **/
/**                # Version 2.0  : from : 01 jul 2008     **/
/**                                 to   : 01 jul 2008     **/
/**                # Version 5.1  : from : 22 nov 2008     **/
/**                                 to   : 27 jun 2010     **/
/**                                                        **/
/************************************************************/

/*
**  The defines and includes.
*/

#define COMMON_MEMORY

#ifndef COMMON_NOMODULE
#include "module.h"
#endif /* COMMON_NOMODULE */
#include "common.h"

/*********************************/
/*                               */
/* The memory handling routines. */
/*                               */
/*********************************/

/* This routine keeps track of the amount of
** allocated memory, and keeps track of the
** maximum allowed.
*/

#ifdef COMMON_MEMORY_TRACE

#define COMMON_MEMORY_SKEW          (MAX ((sizeof (size_t)), (sizeof (double)))) /* For proper alignment */

static size_t           memorysiz = 0;            /*+ Number of allocated bytes        +*/
static size_t           memorymax = 0;            /*+ Maximum amount of allocated data +*/

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
static int              muteflag = 1;             /*+ Flag for mutex initialization +*/
static pthread_mutex_t  mutelocdat;               /*+ Local mutex for updates       +*/
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

void *
memAllocRecord (
size_t                      newsiz)
{
  size_t              newadd;
  byte *              newptr;

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  if (muteflag != 0) {                            /* Unsafe code with respect to race conditions but should work as first allocs are sequential */
    muteflag = 0;
    pthread_mutex_init (&mutelocdat, NULL);       /* Initialize local mutex */
  }
  pthread_mutex_lock (&mutelocdat);               /* Lock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  if ((newptr = malloc (newsiz + COMMON_MEMORY_SKEW)) == NULL) /* Non-zero sizes will guarantee non-NULL pointers */
    newadd = (size_t) 0;
  else {
    newadd = newsiz;
    *((size_t *) newptr) = newsiz;                /* Record size for freeing                */
    newptr += COMMON_MEMORY_SKEW;                 /* Skew pointer while enforcing alignment */
  }

  memorysiz += newadd;
  if (memorymax < memorysiz)
    memorymax = memorysiz;

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_unlock (&mutelocdat);             /* Unlock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  return ((void *) newptr);                       /* Return skewed pointer or NULL */
}

void *
memReallocRecord (
void *                      oldptr,
size_t                      newsiz)
{
  byte *              tmpptr;
  byte *              newptr;
  size_t              oldsiz;
  size_t              newadd;

  tmpptr = ((byte *) oldptr) - COMMON_MEMORY_SKEW;
  oldsiz = *((size_t *) tmpptr);

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_lock (&mutelocdat);               /* Lock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  if ((newptr = realloc (tmpptr, newsiz + COMMON_MEMORY_SKEW)) == NULL)
    newadd = (size_t) 0;
  else {
    newadd = newsiz - oldsiz;                     /* Add difference between the two sizes   */
    *((size_t *) newptr) = newsiz;                /* Record size for freeing                */
    newptr += COMMON_MEMORY_SKEW;                 /* Skew pointer while enforcing alignment */
  }

  memorysiz += newadd;
  if (memorymax < memorysiz)
    memorymax = memorysiz;

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_unlock (&mutelocdat);             /* Unlock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  return ((void *) newptr);                       /* Return skewed pointer or NULL */
}

void
memFreeRecord (
void *                      oldptr)
{
  byte *              tmpptr;
  size_t              oldsiz;

  tmpptr = ((byte *) oldptr) - COMMON_MEMORY_SKEW;
  oldsiz = *((size_t *) tmpptr);

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_lock (&mutelocdat);               /* Lock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  free (tmpptr);
  memorysiz -= oldsiz;

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_unlock (&mutelocdat);             /* Unlock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */
}

size_t
memMax ()
{
  size_t              curmax;

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_lock (&mutelocdat);               /* Lock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  curmax = memorymax;

#if (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD))
  pthread_mutex_unlock (&mutelocdat);             /* Unlock local mutex */
#endif /* (defined (COMMON_PTHREAD) || defined (SCOTCH_PTHREAD)) */

  return (curmax);
}
#endif /* COMMON_MEMORY_TRACE */

/* This routine allocates a set of arrays in
** a single memAlloc()'ed array, the address
** of which is placed in the first argument.
** Arrays to be allocated are described as
** a duplet of ..., &ptr, size, ...,
** terminated by a NULL pointer.
** It returns:
** - !NULL  : pointer to block, all arrays allocated.
** - NULL   : no array allocated.
*/

void *
memAllocGroup (
void **                     memptr,               /*+ Pointer to first argument to allocate +*/
...)
{
  va_list             memlist;                    /* Argument list of the call              */
  byte **             memloc;                     /* Pointer to pointer of current argument */
  size_t              memoff;                     /* Offset value of argument               */
  byte *              blkptr;                     /* Pointer to memory chunk                */

  memoff = 0;
  memloc = (byte **) memptr;                      /* Point to first memory argument */
  va_start (memlist, memptr);                     /* Start argument parsing         */
  while (memloc != NULL) {                        /* As long as not NULL pointer    */
    memoff  = (memoff + (sizeof (double) - 1)) & (~ (sizeof (double) - 1));
    memoff += va_arg (memlist, size_t);
    memloc  = va_arg (memlist, byte **);
  }

  if ((blkptr = (byte *) memAlloc (memoff)) == NULL) { /* If cannot allocate   */
    *memptr = NULL;                               /* Set first pointer to NULL */
    return (NULL);
  }

  memoff = 0;
  memloc = (byte **) memptr;                      /* Point to first memory argument */
  va_start (memlist, memptr);                     /* Restart argument parsing       */
  while (memloc != NULL) {                        /* As long as not NULL pointer    */
    memoff  = (memoff + (sizeof (double) - 1)) & (~ (sizeof (double) - 1)); /* Pad  */
    *memloc = blkptr + memoff;                    /* Set argument address           */
    memoff += va_arg (memlist, size_t);           /* Accumulate padded sizes        */
    memloc  = va_arg (memlist, void *);           /* Get next argument pointer      */
  }

  return ((void *) blkptr);
}

/* This routine reallocates a set of arrays in
** a single memRealloc()'ed array passed as
** first argument, and the address of which
** is placed in the second argument.
** Arrays to be allocated are described as
** a duplet of ..., &ptr, size, ...,
** terminated by a NULL pointer.
** It returns:
** - !NULL  : pointer to block, all arrays allocated.
** - NULL   : no array allocated.
*/

void *
memReallocGroup (
void *                      oldptr,               /*+ Pointer to block to reallocate +*/
...)
{
  va_list             memlist;                    /* Argument list of the call              */
  byte **             memloc;                     /* Pointer to pointer of current argument */
  size_t              memoff;                     /* Offset value of argument               */
  byte *              blkptr;                     /* Pointer to memory chunk                */

  memoff = 0;
  va_start (memlist, oldptr);                     /* Start argument parsing */

  while ((memloc = va_arg (memlist, byte **)) != NULL) { /* As long as not NULL pointer */
    memoff  = (memoff + (sizeof (double) - 1)) & (~ (sizeof (double) - 1)); /* Pad      */
    memoff += va_arg (memlist, size_t);           /* Accumulate padded sizes            */
  }

  if ((blkptr = (byte *) memRealloc (oldptr, memoff)) == NULL) /* If cannot allocate block */
    return (NULL);

  memoff = 0;
  va_start (memlist, oldptr);                     /* Restart argument parsing           */
  while ((memloc = va_arg (memlist, byte **)) != NULL) { /* As long as not NULL pointer */
    memoff  = (memoff + (sizeof (double) - 1)) & (~ (sizeof (double) - 1)); /* Pad      */
    *memloc = blkptr + memoff;                    /* Set argument address               */
    memoff += va_arg (memlist, size_t);           /* Accumulate padded sizes            */
  }

  return ((void *) blkptr);
}

/* This routine computes the offsets of arrays
** of given sizes and types with respect to a
** given base address passed as first argument.
** Arrays the offsets of which are to be computed
** are described as a duplet of ..., &ptr, size, ...,
** terminated by a NULL pointer.
** It returns:
** - !NULL  : in all cases, pointer to the end of
**            the memory area.
*/

void *
memOffset (
void *                      memptr,               /*+ Pointer to base address of memory area +*/
...)
{
  va_list             memlist;                    /* Argument list of the call              */
  byte **             memloc;                     /* Pointer to pointer of current argument */
  size_t              memoff;                     /* Offset value of argument               */

  memoff = 0;
  va_start (memlist, memptr);                     /* Start argument parsing */

  while ((memloc = va_arg (memlist, byte **)) != NULL) { /* As long as not NULL pointer */
    memoff  = (memoff + (sizeof (double) - 1)) & (~ (sizeof (double) - 1));
    *memloc = (byte *) memptr + memoff;           /* Set argument address    */
    memoff += va_arg (memlist, size_t);           /* Accumulate padded sizes */
  }

  return ((void *) ((byte *) memptr + memoff));
}
