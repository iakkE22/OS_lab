#ifndef __KERN_MM_BUDDY_FIT_PMM_H__
#define __KERN_MM_BUDDY_FIT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager buddy2_fit_pmm_manager;

struct Buddy2 *buddy2_new(int size);
int buddy2_alloc(struct Buddy2 *self, int size);
void buddy2_free(struct Buddy2 *self, int offset);

int fix_size(int size);

#endif /* ! __KERN_MM_BUDDY_FIT_PMM_H__ */
