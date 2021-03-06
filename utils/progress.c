/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2005 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov>.
 *  
 *  This file is part of Scrub, a program for erasing disks.
 *  For details, see <http://www.llnl.gov/linux/scrub/>.
 *  
 *  Scrub is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  Scrub is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with Scrub; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

/* ASCII progress bar thingie.
 * Slighthly modified from 'scrub' - use stderr, disable 'batch' mode, and
 * make >100% non-fatal.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "progress.h"

#define PROGRESS_MAGIC  0xabcd1234

struct prog_struct {
    int magic;
    int bars;
    int maxbars;
    int batch;
};

void 
progress_create(prog_t *ctx, int width)
{
    if ((*ctx = (prog_t)malloc(sizeof(struct prog_struct)))) {
        (*ctx)->magic = PROGRESS_MAGIC;
        (*ctx)->maxbars = width - 2;
        (*ctx)->bars = 0;
        (*ctx)->batch = 0;
        if ((*ctx)->batch)
            fprintf(stderr, "|");
        else {
            fprintf(stderr, "|%*s|", (*ctx)->maxbars, "");
            while (width-- > 1)
                fprintf(stderr, "\b");
            fflush(stderr);
        }
    } 
}

void 
progress_destroy(prog_t ctx)
{
    if (ctx) {
        assert(ctx->magic == PROGRESS_MAGIC);
        ctx->magic = 0;
        if (ctx->batch)
            fprintf(stderr, "|\n");
        else
            fprintf(stderr, "\n");
        free(ctx);
    }
}

void 
progress_update(prog_t ctx, double complete)
{
    if (complete > 1.0)
        complete = 1.0;
    if (ctx) {
        assert(ctx->magic == PROGRESS_MAGIC);
        while (ctx->bars < (double)ctx->maxbars * complete) {
            fprintf(stderr, ".");
            if (!ctx->batch)
                fflush(stderr);
            ctx->bars++;
        }
    }
}

#ifdef STAND
int
main(int argc, char *argv[])
{
    prog_t p;
    int i;

    fprintf(stderr, "foo  ");
    progress_create(&p, 70);
    for (i = 1; i <= 100000000L; i++)
        progress_update(p, (double)i/100000000L);
    progress_destroy(p);
    exit(0);
}
#endif

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
