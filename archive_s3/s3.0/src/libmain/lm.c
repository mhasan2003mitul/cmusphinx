/* ====================================================================
 * Copyright (c) 1996-2000 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The names "Sphinx" and "Carnegie Mellon" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. To obtain permission, contact 
 *    sphinx@cs.cmu.edu.
 *
 * 4. Products derived from this software may not be called "Sphinx"
 *    nor may "Sphinx" appear in their names without prior written
 *    permission of Carnegie Mellon University. To obtain permission,
 *    contact sphinx@cs.cmu.edu.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Carnegie
 *    Mellon University (http://www.speech.cs.cmu.edu/)."
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */




/*
 * lm.c -- Disk-based backoff word trigram LM module.
 *
 * 
 * HISTORY
 * 
 * 24-Jun-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added lm_t.log_bg_seg_sz and lm_t.bg_seg_sz.
 * 
 * 13-Feb-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Creating from original S3 version.
 */


#include <libutil/libutil.h>
#include <libio/libio.h>
#include <libmisc/libmisc.h>
#include "lm.h"


#define MIN_PROB_F	((float32)-99.0)


static char *darpa_hdr = "Darpa Trigram LM";


#if 0
int32 lm_delete (lm_t *lm)
{
    int32 i;
    tginfo_t *tginfo, *next_tginfo;
    
    if (lm->fp)
	fclose (lm->fp);
    
    free (lm->ug);

    if (lm->n_bg > 0) {
	if (lm->bg)		/* Memory-based; free all bg */
	    free (lm->bg);
	else {		/* Disk-based; free in-memory bg */
	    for (i = 0; i < lm->n_ug; i++)
		if (lm->membg[i].bg)
		    free (lm->membg[i].bg);
	    free (lm->membg);
	}

	free (lm->bgprob);
    }
    
    if (lm->n_tg > 0) {
	if (lm->tg)		/* Memory-based; free all tg */
	    free (lm->tg);
	for (i = 0; i < lm->n_ug; i++) {	/* Free cached tg access info */
	    for (tginfo = lm->tginfo[i]; tginfo; tginfo = next_tginfo) {
		next_tginfo = tginfo->next;
		if ((! lm->tg) && tginfo->tg)	/* Disk-based; free in-memory tg */
		    free (tginfo->tg);
		free (tginfo);
	    }
	}
	free (lm->tginfo);

	free (lm->tgprob);
	free (lm->tgbowt);
	free (lm->tg_segbase);
    }
    
    for (i = 0; i < lm->n_ug; i++)
	free (lm->wordstr[i]);
    free (lm->wordstr);
    
    free (lm);
    free (lmset[i].name);
    
    for (; i < n_lm-1; i++)
	lmset[i] = lmset[i+1];
    --n_lm;
    
    E_INFO("LM(\"%s\") deleted\n", name);
    
    return (0);
}
#endif


#if _LM_UW_
/* Apply unigram weight; outdated stuff */
static void lm_uw (lm_t *lm, float64 uw)
{
    int32 i, loguw, loguw_, loguniform, p1, p2;

    /* Interpolate unigram probs with uniform PDF, with weight uw */
    loguw = logs3 (uw);
    loguw_ = logs3 (1.0 - uw);
    loguniform = logs3 (1.0/(lm->n_ug-1));
    for (i = 0; i < lm->n_ug; i++) {
	if (strcmp (lm->wordstr[i], START_WORD) != 0) {
	    p1 = lm->ug[i].prob.l + loguw;
	    p2 = loguniform + loguw_;
	    lm->ug[i].prob.l = logs3_add (p1, p2);
	}
    }
}
#endif


static void lm2logs3 (lm_t *lm)
{
    int32 i;

    for (i = 0; i < lm->n_ug; i++) {
	lm->ug[i].prob.l = log10_to_logs3 (lm->ug[i].prob.f);
	lm->ug[i].bowt.l = log10_to_logs3 (lm->ug[i].bowt.f);
    }

#if _LM_UW_
    /* Apply unigram weight; outdated stuff */
    lm_uw (lm, (float64)0.7);
#endif

    for (i = 0; i < lm->n_bgprob; i++)
	lm->bgprob[i].l = log10_to_logs3 (lm->bgprob[i].f);

    if (lm->n_tg > 0) {
	for (i = 0; i < lm->n_tgprob; i++)
	    lm->tgprob[i].l = log10_to_logs3 (lm->tgprob[i].f);
	for (i = 0; i < lm->n_tgbowt; i++)
	    lm->tgbowt[i].l = log10_to_logs3 (lm->tgbowt[i].f);
    }
}


void lm_set_param (lm_t *lm, float64 lw, float64 wip)
{
    int32 i, iwip;
    float64 f;
    
    if (lw <= 0.0)
	E_FATAL("lw = %e\n", lw);
    if (wip <= 0.0)
	E_FATAL("wip = %e\n", wip);
    iwip = logs3 (wip);
    
    f = lw / lm->lw;
    
    for (i = 0; i < lm->n_ug; i++) {
	lm->ug[i].prob.l = (int32)((lm->ug[i].prob.l - lm->wip) * f) + iwip;
	lm->ug[i].bowt.l = (int32)(lm->ug[i].bowt.l * f);
    }

    for (i = 0; i < lm->n_bgprob; i++)
	lm->bgprob[i].l = (int32)((lm->bgprob[i].l - lm->wip) * f) + iwip;

    if (lm->n_tg > 0) {
	for (i = 0; i < lm->n_tgprob; i++)
	    lm->tgprob[i].l = (int32)((lm->tgprob[i].l - lm->wip) * f) + iwip;
	for (i = 0; i < lm->n_tgbowt; i++)
	    lm->tgbowt[i].l = (int32)(lm->tgbowt[i].l * f);
    }

    lm->lw = (float32) lw;
    lm->wip = iwip;
}


static int32 lm_fread_int32 (lm_t *lm)
{
    int32 val;
    
    if (fread (&val, sizeof(int32), 1, lm->fp) != 1)
	E_FATAL("fread failed\n");
    if (lm->byteswap)
	SWAP_INT32(&val);
    return (val);
}


/*
 * Read LM dump (<lmname>.DMP) file and make it the current LM.
 * Same interface as lm_read except that the filename refers to a .DMP file.
 */
static lm_t *lm_read_dump (char *file, float64 lw, float64 wip)
{
    lm_t *lm;
    int32 i, j, k, vn;
    char str[1024];
    char *tmp_word_str;
    s3lmwid_t startwid, endwid;
    
    lm = (lm_t *) ckd_calloc (1, sizeof(lm_t));
    
    if ((lm->fp = fopen (file, "rb")) == NULL)
	E_FATAL_SYSTEM("fopen(%s,rb) failed\n", file);
    
    /* Standard header string-size; set byteswap flag based on this */
    if (fread (&k, sizeof(int32), 1, lm->fp) != 1)
	E_FATAL("fread(%s) failed\n", file);
    if ((size_t)k == strlen(darpa_hdr)+1)
	lm->byteswap = 0;
    else {
	SWAP_INT32(&k);
	if ((size_t)k == strlen(darpa_hdr)+1)
	    lm->byteswap = 1;
	else {
	    SWAP_INT32(&k);
	    E_FATAL("Bad magic number: %d(%08x), not an LM dumpfile??\n", k, k);
	}
    }

    /* Read and verify standard header string */
    if (fread (str, sizeof (char), k, lm->fp) != (size_t)k)
	E_FATAL("fread(%s) failed\n", file);
    if (strncmp (str, darpa_hdr, k) != 0)
	E_FATAL("Bad header\n");

    /* Original LM filename string size and string */
    k = lm_fread_int32 (lm);
    if ((k < 1) || (k > 1024))
	E_FATAL("Bad original filename size: %d\n", k);
    if (fread (str, sizeof (char), k, lm->fp) != (size_t)k)
	E_FATAL("fread(%s) failed\n", file);

    /* Version#.  If present (must be <= 0); otherwise it's actually the unigram count */
    vn = lm_fread_int32 (lm);
    if (vn <= 0) {
	/* Read and skip orginal file timestamp; (later compare timestamps) */
	k = lm_fread_int32 (lm);

	/* Read and skip format description */
	for (;;) {
	    if ((k = lm_fread_int32 (lm)) == 0)
		break;
	    if (fread (str, sizeof(char), k, lm->fp) != (size_t)k)
		E_FATAL("fread(%s) failed\n", file);
	}

	/* Read log_bg_seg_sz if present */
	if (vn <= -2) {
	    k = lm_fread_int32 (lm);
	    if ((k < 1) || (k > 15))
		E_FATAL("log2(bg_seg_sz) outside range 1..15\n", k);
	    lm->log_bg_seg_sz = k;
	} else
	    lm->log_bg_seg_sz = LOG2_BG_SEG_SZ;	/* Default */

	/* Read #ug */
	lm->n_ug = lm_fread_int32 (lm);
    } else {
	/* No version number, actually a unigram count */
	lm->n_ug = vn;
	lm->log_bg_seg_sz = LOG2_BG_SEG_SZ;	/* Default */
    }
    if ((lm->n_ug <= 0) || (lm->n_ug >= MAX_LMWID))
	E_FATAL("Bad #unigrams: %d (must be >0, <%d\n", lm->n_ug, MAX_LMWID);

    lm->bg_seg_sz = 1 << lm->log_bg_seg_sz;

    /* #bigrams */
    lm->n_bg = lm_fread_int32 (lm);
    if (lm->n_bg < 0)
	E_FATAL("Bad #bigrams: %d\n", lm->n_bg);

    /* #trigrams */
    lm->n_tg = lm_fread_int32 (lm);
    if (lm->n_tg < 0)
	E_FATAL("Bad #trigrams: %d\n", lm->n_tg);

    /* Read unigrams; remember sentinel ug at the end! */
    lm->ug = (ug_t *) ckd_calloc (lm->n_ug+1, sizeof(ug_t));
    if (fread (lm->ug, sizeof(ug_t), lm->n_ug+1, lm->fp) != (size_t)(lm->n_ug+1))
	E_FATAL("fread(%s) failed\n", file);
    if (lm->byteswap)
	for (i = 0; i <= lm->n_ug; i++) {
	    SWAP_INT32(&(lm->ug[i].prob.l));
	    SWAP_INT32(&(lm->ug[i].bowt.l));
	    SWAP_INT32(&(lm->ug[i].firstbg));
	}
    E_INFO("%8d unigrams\n", lm->n_ug);

    /* Space for in-memory bigrams/trigrams info; FOR NOW, DISK-BASED LM OPTION ONLY!! */
    lm->bg = NULL;
    lm->tg = NULL;

    /* Skip bigrams; remember sentinel at the end */
    if (lm->n_bg > 0) {
	lm->bgoff = ftell (lm->fp);
	fseek (lm->fp, (lm->n_bg+1) * sizeof(bg_t), SEEK_CUR);
	E_INFO("%8d bigrams [on disk]\n", lm->n_bg);

	lm->membg = (membg_t *) ckd_calloc (lm->n_ug, sizeof(membg_t));
    }
    
    /* Skip trigrams */
    if (lm->n_tg > 0) {
	lm->tgoff = ftell (lm->fp);
	fseek (lm->fp, lm->n_tg * sizeof(tg_t), SEEK_CUR);
	E_INFO("%8d trigrams [on disk]\n", lm->n_tg);

	lm->tginfo = (tginfo_t **) ckd_calloc (lm->n_ug, sizeof(tginfo_t *));
    }
    
    if (lm->n_bg > 0) {
	/* Bigram probs table size */
	lm->n_bgprob = lm_fread_int32 (lm);
	if ((lm->n_bgprob <= 0) || (lm->n_bgprob > 65536))
	    E_FATAL("Bad bigram prob table size: %d\n", lm->n_bgprob);
	
	/* Allocate and read bigram probs table */
	lm->bgprob = (lmlog_t *) ckd_calloc (lm->n_bgprob, sizeof (lmlog_t));
	if (fread(lm->bgprob, sizeof(lmlog_t), lm->n_bgprob, lm->fp) !=
	    (size_t)lm->n_bgprob)
	    E_FATAL("fread(%s) failed\n", file);
	if (lm->byteswap) {
	    for (i = 0; i < lm->n_bgprob; i++)
		SWAP_INT32(&(lm->bgprob[i].l));
	}

	E_INFO("%8d bigram prob entries\n", lm->n_bgprob);
    }

    if (lm->n_tg > 0) {
	/* Trigram bowt table size */
	lm->n_tgbowt = lm_fread_int32 (lm);
	if ((lm->n_tgbowt <= 0) || (lm->n_tgbowt > 65536))
	    E_FATAL("Bad trigram bowt table size: %d\n", lm->n_tgbowt);
	
	/* Allocate and read trigram bowt table */
	lm->tgbowt = (lmlog_t *) ckd_calloc (lm->n_tgbowt, sizeof (lmlog_t));
	if (fread (lm->tgbowt, sizeof (lmlog_t), lm->n_tgbowt, lm->fp) !=
	    (size_t)lm->n_tgbowt)
	    E_FATAL("fread(%s) failed\n", file);
	if (lm->byteswap) {
	    for (i = 0; i < lm->n_tgbowt; i++)
		SWAP_INT32(&(lm->tgbowt[i].l));
	}
	E_INFO("%8d trigram bowt entries\n", lm->n_tgbowt);

	/* Trigram prob table size */
	lm->n_tgprob = lm_fread_int32 (lm);
	if ((lm->n_tgprob <= 0) || (lm->n_tgprob > 65536))
	    E_FATAL("Bad trigram bowt table size: %d\n", lm->n_tgprob);
	
	/* Allocate and read trigram bowt table */
	lm->tgprob = (lmlog_t *) ckd_calloc (lm->n_tgprob, sizeof (lmlog_t));
	if (fread (lm->tgprob, sizeof (lmlog_t), lm->n_tgprob, lm->fp) !=
	    (size_t)lm->n_tgprob)
	    E_FATAL("fread(%s) failed\n", file);
	if (lm->byteswap) {
	    for (i = 0; i < lm->n_tgprob; i++)
		SWAP_INT32(&(lm->tgprob[i].l));
	}
	E_INFO("%8d trigram prob entries\n", lm->n_tgprob);

	/* Trigram seg table size */
	k = lm_fread_int32 (lm);
	if (k != (lm->n_bg+1)/lm->bg_seg_sz+1)
	    E_FATAL("Bad trigram seg table size: %d\n", k);
	
	/* Allocate and read trigram seg table */
	lm->tg_segbase = (int32 *) ckd_calloc (k, sizeof(int32));
	if (fread (lm->tg_segbase, sizeof(int32), k, lm->fp) != (size_t)k)
	    E_FATAL("fread(%s) failed\n", file);
	if (lm->byteswap) {
	    for (i = 0; i < k; i++)
		SWAP_INT32(&(lm->tg_segbase[i]));
	}
	E_INFO("%8d trigram segtable entries (%d segsize)\n", k, lm->bg_seg_sz);
    }

    /* Read word string names */
    k = lm_fread_int32 (lm);
    if (k <= 0)
	E_FATAL("Bad wordstrings size: %d\n", k);
    
    tmp_word_str = (char *) ckd_calloc (k, sizeof (char));
    if (fread (tmp_word_str, sizeof(char), k, lm->fp) != (size_t)k)
	E_FATAL("fread(%s) failed\n", file);

    /* First make sure string just read contains ucount words (PARANOIA!!) */
    for (i = 0, j = 0; i < k; i++)
	if (tmp_word_str[i] == '\0')
	    j++;
    if (j != lm->n_ug)
	E_FATAL("Bad #words: %d\n", j);

    /* Break up string just read into words */
    startwid = endwid = BAD_LMWID;
    lm->wordstr = (char **) ckd_calloc (lm->n_ug, sizeof(char *));
    j = 0;
    for (i = 0; i < lm->n_ug; i++) {
	if (strcmp (tmp_word_str+j, START_WORD) == 0)
	    startwid = i;
	else if (strcmp (tmp_word_str+j, FINISH_WORD) == 0)
	    endwid = i;

	lm->wordstr[i] = (char *) ckd_salloc (tmp_word_str+j);
	
	j += strlen(tmp_word_str+j) + 1;
    }
    free (tmp_word_str);
    E_INFO("%8d word strings\n", i);
    
    /* Force ugprob(<s>) = MIN_PROB_F */
    if (IS_LMWID(startwid))
	lm->ug[startwid].prob.f = MIN_PROB_F;
    else
	E_WARN("%s not in %s\n", START_WORD, file);
    
    /* Force bowt(</s>) = MIN_PROB_F */
    if (IS_LMWID(endwid))
	lm->ug[endwid].bowt.f = MIN_PROB_F;
    else
	E_WARN("%s not in %s\n", FINISH_WORD, file);
    
    lm2logs3 (lm);
    lm->lw = 1.0;	/* The initial settings for lw and wip */
    lm->wip = 0;	/* logs3(1.0) */

    /* Apply the new lw and wip values */
    lm_set_param (lm, lw, wip);
    
    return lm;
}


lm_t *lm_read (char *file, float64 lw, float64 wip)
{
    int32 u;
    lm_t *lm;
    
    if (! file)
	E_FATAL("No LM file\n");
    
    E_INFO ("Reading LM file: %s\n", file);
    
    /* For now, only dump files can be read; they are created offline */
    lm = lm_read_dump (file, lw, wip);

    for (u = 0; u < lm->n_ug; u++)
	lm->ug[u].dictwid = BAD_WID;
    
    return lm;
}


/*
 * Free stale bigram and trigram info, those not used since last reset.
 */
void lm_cache_reset (lm_t *lm)
{
    int32 i, n_bgfree, n_tgfree;
    tginfo_t *tginfo, *next_tginfo, *prev_tginfo;
    
    n_bgfree = n_tgfree = 0;
    
    if ((lm->n_bg > 0) && (! lm->bg)) {	/* Disk-based; free "stale" bigrams */
	for (i = 0; i < lm->n_ug; i++) {
	    if (lm->membg[i].bg && (! lm->membg[i].used)) {
		lm->n_bg_inmem -= lm->ug[i+1].firstbg - lm->ug[i].firstbg;

		free (lm->membg[i].bg);
		lm->membg[i].bg = NULL;
		n_bgfree++;
	    }

	    lm->membg[i].used = 0;
	}
    }
    
    if (lm->n_tg > 0) {
	for (i = 0; i < lm->n_ug; i++) {
	    prev_tginfo = NULL;
	    for (tginfo = lm->tginfo[i]; tginfo; tginfo = next_tginfo) {
		next_tginfo = tginfo->next;
		
		if (! tginfo->used) {
		    if ((! lm->tg) && tginfo->tg) {
			lm->n_tg_inmem -= tginfo->n_tg;
			free (tginfo->tg);
			n_tgfree++;
		    }
		    
		    free (tginfo);
		    if (prev_tginfo)
			prev_tginfo->next = next_tginfo;
		    else
			lm->tginfo[i] = next_tginfo;
		} else {
		    tginfo->used = 0;
		    prev_tginfo = tginfo;
		}
	    }
	}
    }

    if ((n_tgfree > 0) || (n_bgfree > 0)) {
	E_INFO("%d tg frees, %d in mem; %d bg frees, %d in mem\n",
	       n_tgfree, lm->n_tg_inmem, n_bgfree, lm->n_bg_inmem);
    }
}


void lm_cache_stats_dump (lm_t *lm)
{
    E_INFO("%8d tg(), %d bo; %d fills, %d in mem (%.1f%%)\n",
	   lm->n_tg_score, lm->n_tg_bo, lm->n_tg_fill, lm->n_tg_inmem,
	   (lm->n_tg_inmem*100.0)/(lm->n_tg+1));
    E_INFO("%8d bg(), %d bo; %d fills, %d in mem (%.1f%%)\n",
	   lm->n_bg_score, lm->n_bg_bo, lm->n_bg_fill, lm->n_bg_inmem,
	   (lm->n_bg_inmem*100.0)/(lm->n_bg+1));
    
    lm->n_tg_fill = 0;
    lm->n_tg_score = 0;
    lm->n_tg_bo = 0;
    lm->n_bg_fill = 0;
    lm->n_bg_score = 0;
    lm->n_bg_bo = 0;
}


int32 lm_ug_score (lm_t *lm, s3lmwid_t wid)
{
    if (NOT_LMWID(wid) || (wid >= lm->n_ug))
	E_FATAL("Bad argument (%d) to lm_ug_score\n", wid);

    return (lm->ug[wid].prob.l);
}


int32 lm_uglist (lm_t *lm, ug_t **ugptr)
{
    *ugptr = lm->ug;
    return (lm->n_ug);
}


/*
 * Load bigrams for the given unigram (LMWID) lw1 from disk into memory
 */
static void load_bg (lm_t *lm, s3lmwid_t lw1)
{
    int32 i, n, b;
    bg_t *bg;
    
    b = lm->ug[lw1].firstbg;		/* Absolute first bg index for ug lw1 */
    n = lm->ug[lw1+1].firstbg - b;	/* Not including guard/sentinel */
    
    bg = lm->membg[lw1].bg = (bg_t *) ckd_calloc (n+1, sizeof(bg_t));
    
    if (fseek (lm->fp, lm->bgoff + b*sizeof(bg_t), SEEK_SET) < 0)
	E_FATAL_SYSTEM ("fseek failed\n");
    
    /* Need to read n+1 because obtaining tg count for one bg also depends on next bg */
    if (fread (bg, sizeof(bg_t), n+1, lm->fp) != (size_t)(n+1))
	E_FATAL("fread failed\n");
    if (lm->byteswap) {
	for (i = 0; i <= n; i++) {
	    SWAP_INT16(&(bg[i].wid));
	    SWAP_INT16(&(bg[i].probid));
	    SWAP_INT16(&(bg[i].bowtid));
	    SWAP_INT16(&(bg[i].firsttg));
	}
    }
    
    lm->n_bg_fill++;
    lm->n_bg_inmem += n;
}


#define BINARY_SEARCH_THRESH	16

/* Locate a specific bigram within a bigram list */
static int32 find_bg (bg_t *bg, int32 n, s3lmwid_t w)
{
    int32 i, b, e;
    
    /* Binary search until segment size < threshold */
    b = 0;
    e = n;
    while (e-b > BINARY_SEARCH_THRESH) {
	i = (b+e)>>1;
	if (bg[i].wid < w)
	    b = i+1;
	else if (bg[i].wid > w)
	    e = i;
	else
	    return i;
    }

    /* Linear search within narrowed segment */
    for (i = b; (i < e) && (bg[i].wid != w); i++);
    return ((i < e) ? i : -1);
}


int32 lm_bglist (lm_t *lm, s3lmwid_t w1, bg_t **bgptr, int32 *bowt)
{
    int32 n;

    if (NOT_LMWID(w1) || (w1 >= lm->n_ug))
	E_FATAL("Bad w1 argument (%d) to lm_bglist\n", w1);

    n = (lm->n_bg > 0) ? lm->ug[w1+1].firstbg - lm->ug[w1].firstbg : 0;
    
    if (n > 0) {
	if (! lm->membg[w1].bg)
	    load_bg (lm, w1);
	lm->membg[w1].used = 1;

	*bgptr = lm->membg[w1].bg;
	*bowt = lm->ug[w1].bowt.l;
    } else {
	*bgptr = NULL;
	*bowt = 0;
    }
    
    return (n);
}


int32 lm_bg_score (lm_t *lm, s3lmwid_t w1, s3lmwid_t w2)
{
    int32 i, n, score;
    bg_t *bg;

    if ((lm->n_bg == 0) || (NOT_LMWID(w1)))
	return (lm_ug_score (lm, w2));

    lm->n_bg_score++;

    if (NOT_LMWID(w2) || (w2 >= lm->n_ug))
	E_FATAL("Bad w2 argument (%d) to lm_bg_score\n", w2);
    
    n = lm->ug[w1+1].firstbg - lm->ug[w1].firstbg;
    
    if (n > 0) {
	if (! lm->membg[w1].bg)
	    load_bg (lm, w1);
	lm->membg[w1].used = 1;
	bg = lm->membg[w1].bg;

	i = find_bg (bg, n, w2);
    } else
	i = -1;
    
    if (i >= 0)
	score = lm->bgprob[bg[i].probid].l;
    else {
	lm->n_bg_bo++;
	score = lm->ug[w1].bowt.l + lm->ug[w2].prob.l;
    }

#if 0
    printf ("      %5d %5d -> %8d\n", lw1, lw2, score);
#endif

    return (score);
}


static void load_tg (lm_t *lm, s3lmwid_t lw1, s3lmwid_t lw2)
{
    int32 i, n, b, t;
    bg_t *bg;
    tg_t *tg;
    tginfo_t *tginfo;
    
    /* First allocate space for tg information for bg lw1,lw2 */
    tginfo = (tginfo_t *) ckd_malloc (sizeof(tginfo_t));
    tginfo->w1 = lw1;
    tginfo->tg = NULL;
    tginfo->next = lm->tginfo[lw2];
    lm->tginfo[lw2] = tginfo;
    
    /* Locate bigram lw1,lw2 */

    b = lm->ug[lw1].firstbg;
    n = lm->ug[lw1+1].firstbg - b;
    
    /* Make sure bigrams for lw1, if any, loaded into memory */
    if (n > 0) {
	if (! lm->membg[lw1].bg)
	    load_bg (lm, lw1);
	lm->membg[lw1].used = 1;
	bg = lm->membg[lw1].bg;
    }

    /* At this point, n = #bigrams for lw1 */
    if ((n > 0) && ((i = find_bg (bg, n, lw2)) >= 0)) {
	tginfo->bowt = lm->tgbowt[bg[i].bowtid].l;
	
	/* Find t = Absolute first trigram index for bigram lw1,lw2 */
	b += i;			/* b = Absolute index of bigram lw1,lw2 on disk */
	t = lm->tg_segbase[b >> lm->log_bg_seg_sz];
	t += bg[i].firsttg;
	
	/* Find #tg for bigram w1,w2 */
	n = lm->tg_segbase[(b+1) >> lm->log_bg_seg_sz];
	n += bg[i+1].firsttg;
	n -= t;
	tginfo->n_tg = n;
    } else {			/* No bigram w1,w2 */
	tginfo->bowt = 0;
	n = tginfo->n_tg = 0;
    }
    
    /* At this point, n = #trigrams for lw1,lw2.  Read them in */
    if (n > 0) {
	tg = tginfo->tg = (tg_t *) ckd_calloc (n, sizeof(tg_t));
	if (fseek (lm->fp, lm->tgoff + t*sizeof(tg_t), SEEK_SET) < 0)
	    E_FATAL_SYSTEM("fseek failed\n");

	if (fread (tg, sizeof(tg_t), n, lm->fp) != (size_t)n)
	    E_FATAL("fread(tg, %d at %d) failed\n", n, lm->tgoff);
	if (lm->byteswap) {
	    for (i = 0; i < n; i++) {
		SWAP_INT16(&(tg[i].wid));
		SWAP_INT16(&(tg[i].probid));
	    }
	}
    }
    
    lm->n_tg_fill++;
    lm->n_tg_inmem += n;
}


/* Similar to find_bg */
static int32 find_tg (tg_t *tg, int32 n, s3lmwid_t w)
{
    int32 i, b, e;
    
    b = 0;
    e = n;
    while (e-b > BINARY_SEARCH_THRESH) {
	i = (b+e)>>1;
	if (tg[i].wid < w)
	    b = i+1;
	else if (tg[i].wid > w)
	    e = i;
	else
	    return i;
    }
    
    for (i = b; (i < e) && (tg[i].wid != w); i++);
    return ((i < e) ? i : -1);
}


int32 lm_tglist (lm_t *lm, s3lmwid_t lw1, s3lmwid_t lw2, tg_t **tgptr, int32 *bowt)
{
    tginfo_t *tginfo, *prev_tginfo;

    if (lm->n_tg <= 0) {
	*tgptr = NULL;
	*bowt = 0;
	return 0;
    }
    
    if (NOT_LMWID(lw1) || (lw1 >= lm->n_ug))
	E_FATAL("Bad lw1 argument (%d) to lm_tglist\n", lw1);
    if (NOT_LMWID(lw2) || (lw2 >= lm->n_ug))
	E_FATAL("Bad lw2 argument (%d) to lm_tglist\n", lw2);

    prev_tginfo = NULL;
    for (tginfo = lm->tginfo[lw2]; tginfo; tginfo = tginfo->next) {
	if (tginfo->w1 == lw1)
	    break;
	prev_tginfo = tginfo;
    }
    
    if (! tginfo) {
    	load_tg (lm, lw1, lw2);
	tginfo = lm->tginfo[lw2];
    } else if (prev_tginfo) {
	prev_tginfo->next = tginfo->next;
	tginfo->next = lm->tginfo[lw2];
	lm->tginfo[lw2] = tginfo;
    }
    tginfo->used = 1;

    *tgptr = tginfo->tg;
    *bowt = tginfo->bowt;

    return (tginfo->n_tg);
}


int32 lm_tg_score (lm_t *lm, s3lmwid_t lw1, s3lmwid_t lw2, s3lmwid_t lw3)
{
    int32 i, n, score;
    tg_t *tg;
    tginfo_t *tginfo, *prev_tginfo;
    
    if ((lm->n_tg == 0) || (NOT_LMWID(lw1)))
	return (lm_bg_score (lm, lw2, lw3));

    lm->n_tg_score++;

    if (NOT_LMWID(lw1) || (lw1 >= lm->n_ug))
	E_FATAL("Bad lw1 argument (%d) to lm_tg_score\n", lw1);
    if (NOT_LMWID(lw2) || (lw2 >= lm->n_ug))
	E_FATAL("Bad lw2 argument (%d) to lm_tg_score\n", lw2);
    if (NOT_LMWID(lw3) || (lw3 >= lm->n_ug))
	E_FATAL("Bad lw3 argument (%d) to lm_tg_score\n", lw3);
    
    prev_tginfo = NULL;
    for (tginfo = lm->tginfo[lw2]; tginfo; tginfo = tginfo->next) {
	if (tginfo->w1 == lw1)
	    break;
	prev_tginfo = tginfo;
    }
    
    if (! tginfo) {
    	load_tg (lm, lw1, lw2);
	tginfo = lm->tginfo[lw2];
    } else if (prev_tginfo) {
	prev_tginfo->next = tginfo->next;
	tginfo->next = lm->tginfo[lw2];
	lm->tginfo[lw2] = tginfo;
    }

    tginfo->used = 1;
    
    /* Trigrams for w1,w2 now in memory; look for w1,w2,w3 */
    n = tginfo->n_tg;
    tg = tginfo->tg;
    if ((i = find_tg (tg, n, lw3)) >= 0)
	score = lm->tgprob[tg[i].probid].l;
    else {
	lm->n_tg_bo++;
	score = tginfo->bowt + lm_bg_score(lm, lw2, lw3);
    }

#if 0
    printf ("%5d %5d %5d -> %8d\n", lw1, lw2, lw3, score);
#endif

    return (score);
}


#if (_LM_TEST_)
s3lmwid_t lm_wid (lm_t *lm, char *word)
{
    int32 i;
    
    for (i = 0; i < lm->n_ug; i++)
	if (strcmp (lm->wordstr[i], word) == 0)
	    return ((s3lmwid_t) i);
    
    return BAD_LMWID;
}


static int32 sentence_lmscore (lm_t *lm, char *line)
{
    char *word[1024];
    s3lmwid_t w[1024];
    int32 nwd, score, tgscr;
    int32 i, j;
    
    if ((nwd = str2words (line, word, 1020)) < 0)
	E_FATAL("Increase word[] and w[] arrays size\n");
    
    w[0] = BAD_LMWID;
    w[1] = lm_wid (lm, START_WORD);
    if (NOT_LMWID(w[1]))
	E_FATAL("Unknown word: %s\n", START_WORD);
    
    for (i = 0; i < nwd; i++) {
	w[i+2] = lm_wid (lm, word[i]);
	if (NOT_LMWID(w[i+2])) {
	    E_ERROR("Unknown word: %s\n", word[i]);
	    return 0;
	}
    }

    w[i+2] = lm_wid (lm, FINISH_WORD);
    if (NOT_LMWID(w[i+2]))
	E_FATAL("Unknown word: %s\n", FINISH_WORD);
    
    score = 0;
    for (i = 0, j = 2; i <= nwd; i++, j++) {
	tgscr = lm_tg_score (lm, w[j-2], w[j-1], w[j]);
	score += tgscr;
	printf ("\t%10d %s\n", tgscr, lm->wordstr[w[j]]);
    }
    
    return (score);
}


main (int32 argc, char *argv[])
{
    char line[4096];
    int32 score, k;
    lm_t *lm;
    
    if (argc < 2)
	E_FATAL("Usage: %s <LMdumpfile>\n", argv[0]);

    logs3_init (1.0001);
    lm = lm_read (argv[1], 9.5, 0.2);

    for (;;) {
	printf ("> ");
	if (fgets (line, sizeof(line), stdin) == NULL)
	    break;
	
	score = sentence_lmscore (lm, line);

	k = strlen(line);
	if (line[k-1] == '\n')
	    line[k-1] = '\0';
	printf ("LMScr(%s) = %d\n", line, score);
    }
}
#endif
