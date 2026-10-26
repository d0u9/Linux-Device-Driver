/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MAIN_H
#define _MAIN_H

#define MODULE_NAME		"simple"
#define DEV_NR			2

#define SIMPLE_PAGES		8		/* buffer size of each device */
#define SIMPLE_SIZE		(SIMPLE_PAGES * PAGE_SIZE)
#define SIMPLE_PATTERN		0xdead0000	/* page i holds PATTERN + i */

#endif
