/***************************************************************************
 caltool.h -- Public interface for iCalendar tools in caltool.c

 Created: Jan.13 2016
 Author: Nick Major #0879292
 Contact: nmajor@mail.uoguelph.ca

 ***************************************************************************/

#ifndef CALTOOL_H
#define CALTOOL_H A2_RevA

#define _GNU_SOURCE        // for getdate_r
#define _XOPEN_SOURCE      // for strptime
#include "calutil.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

/* Symbols used to send options to command execution modules */

typedef enum {
    OEVENT,     // events
    OPROP,      // properties
    OTODO,      // to-do items
} CalOpt;

/* iCalendar tool functions */

CalStatus calInfo( const CalComp *comp, int lines, FILE *const txtfile );
CalStatus calExtract( const CalComp *comp, CalOpt kind, FILE *const txtfile );
CalStatus calFilter( const CalComp *comp, CalOpt content, time_t datefrom, time_t dateto, FILE *const icsfile );
CalStatus calCombine( const CalComp *comp1, const CalComp *comp2, FILE *const icsfile );

#endif
