#include <stdint.h>

#include "v8_mmu.h"

extern uint8_t __ttb0_l1;
extern uint8_t __ttb0_l2_periph;

#define update_l2_item(t, va, pa) \
    do { \
        uint64_t *tab = t; \
        *(tab + (((va) >> 21) & 0x1ff)) = ((pa) & (~0xfff)) | (TT_S1_ATTR_BLOCK | (2 << TT_S1_ATTR_MATTR_LSB) | TT_S1_ATTR_NS | TT_S1_ATTR_AP_RW_PL1 | TT_S1_ATTR_AF | TT_S1_ATTR_nG); \
    } while (0)

/* only map 2M address space */
void mmap_dev(uint64_t va, uint64_t pa)
{
    uint64_t *tab1 = &__ttb0_l1;
    uint64_t *tab2 = &__ttb0_l2_periph;

    // check item of table level 1
    if (0 == *(tab1 + ((va >> 30) & 0xf))) {
	// level 2
	update_l2_item(tab2, va, pa);
        __asm__ __volatile__("dsb ish");

	// level 1
        *(tab1 + ((va >> 30) & 0xf)) = (uint64_t) tab2 | TT_S1_ATTR_TABLE;
        __asm__ __volatile__("dsb ish");
    } else {
	// level 2, notice tab2 is get from tab1, so tab2 is physical address
	tab2 = *(tab1 + ((va >> 30) & 0xf)) & (~0xfff);
	// we could access physcial address beacause of shadow map
        update_l2_item(tab2, va, pa);
	__asm__ __volatile__("dsb ish");
    }

    // flush tlb
    __asm__ __volatile__("tlbi VMALLE1");
}

