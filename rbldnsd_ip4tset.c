/* $Id$
 * ip4tset dataset type: IP4 addresses (ranges), with A and TXT
 * values for every individual entry.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rbldnsd.h"

struct dsdata {
  unsigned n;		/* count */
  unsigned a;		/* allocated (only for loading) */
  unsigned f;		/* how much to allocate next time */
  ip4addr_t *e;		/* array of entries */
  const char *def_rr;	/* default A and TXT RRs */
};

definedstype(ip4tset, DSTF_IP4REV, "(trivial) set of ip4 addresses");

static void ds_ip4tset_reset(struct dsdata *dsd, int UNUSED unused_freeall) {
 if (dsd->e) {
    free(dsd->e);
    dsd->e = NULL;
    dsd->n = dsd->a = 0;
  }
  dsd->def_rr = NULL;
}

static void ds_ip4tset_start(struct dataset UNUSED *unused_ds) {
}

static int
ds_ip4tset_line(struct dataset *ds, char *s, int lineno) {
  struct dsdata *dsd = ds->ds_dsd;
  ip4addr_t a;

  if (*s == ':') {
    if (!dsd->def_rr) {
      unsigned rrl;
      const char *rr;
      if (!(rrl = parse_a_txt(lineno, s, &rr, def_rr)))
        return 1;
      if (!(dsd->def_rr = mp_dmemdup(ds->ds_mp, rr, rrl)))
        return 0;
    }
    return 1;
  }

  if (!ip4addr(s, &a, &s) ||
      (*s && !ISSPACE(*s) && !ISCOMMENT(*s) && *s != ':')) {
    dswarn(lineno, "invalid address");
    return 1;
  }

  if (dsd->n >= dsd->a) {
    ip4addr_t *e = dsd->e;
    if (!dsd->a)
      dsd->a = dsd->f ? dsd->f : 64;
    else
      dsd->a <<= 1;
    e = trealloc(ip4addr_t, e, dsd->a);
    if (!e)
      return 0;
    dsd->e = e;
  }

  dsd->e[dsd->n++] = a;

  return 1;
}

static void ds_ip4tset_finish(struct dataset *ds) {
  struct dsdata *dsd = ds->ds_dsd;
  ip4addr_t *e = dsd->e;
  unsigned n = dsd->n;

  if (n) {
    dsd->f = dsd->a;
    while((dsd->f >> 1) >= n)
      dsd->f >>= 1;

#   define QSORT_TYPE ip4addr_t
#   define QSORT_BASE e
#   define QSORT_NELT n
#   define QSORT_LT(a,b) *a < *b
#   include "qsort.c"

#define ip4tset_eeq(a,b) a == b
    REMOVE_DUPS(ip4addr_t, e, n, ip4tset_eeq);
    SHRINK_ARRAY(ip4addr_t, e, n, dsd->a);
    dsd->e = e;
    dsd->n = n;
  }

  if (!dsd->def_rr) dsd->def_rr = def_rr;
  dsloaded("cnt=%u", n);
}

static int
ds_ip4tset_find(const ip4addr_t *e, int b, ip4addr_t q) {
  int a = 0, m;
  --b;
  while(a <= b) {
    if (e[(m = (a + b) >> 1)] == q) return 1;
    else if (e[m] < q) a = m + 1;
    else b = m - 1;
  }
  return 0;
}

static int
ds_ip4tset_query(const struct dataset *ds, const struct dnsqinfo *qi,
                struct dnspacket *pkt) {
  const struct dsdata *dsd = ds->ds_dsd;
  const char *ipsubst;

  if (!qi->qi_ip4valid) return 0;

  if (!dsd->n || !ds_ip4tset_find(dsd->e, dsd->n, qi->qi_ip4))
    return 0;

  ipsubst = (qi->qi_tflag & NSQUERY_TXT) ? ip4atos(qi->qi_ip4) : NULL;
  addrr_a_txt(pkt, qi->qi_tflag, dsd->def_rr, ipsubst, ds);

  return 1;
}

static void
ds_ip4tset_dump(const struct dataset *ds,
               const unsigned char UNUSED *unused_odn,
               FILE *f) {
  ip4addr_t a;
  const ip4addr_t *e, *t;
  const struct dsdata *dsd = ds->ds_dsd;
  char name[4*3+3+1];
  for(e = dsd->e, t = e + dsd->n; e < t; ++e) {
    a = *e;
    sprintf(name, "%u.%u.%u.%u",
            a & 255, (a >> 8) & 255, (a >> 16) & 255, (a >> 24));
    dump_a_txt(name, dsd->def_rr, ip4atos(a), ds, f);
  }
}
