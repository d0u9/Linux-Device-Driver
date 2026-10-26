/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#include <linux/mm_types.h>
#include <linux/mutex.h>

#define MODULE_NAME		"zero_copy"
#define ZC_MAX_PAGES		16	/* largest buffer that can be pinned */

/* One per open file. */
struct zc_ctx {
	struct mutex lock;		/* protects pages and npages */
	struct page **pages;		/* the pinned user pages, or NULL */
	int npages;
};

#endif
