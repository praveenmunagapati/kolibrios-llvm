#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 */

#define usleep_range(min, max)  udelay(max)

#endif /* defined(_LINUX_DELAY_H) */
