#include <stdint.h>

#include "v8_mmu.h"

extern uint8_t __ttb0_l1;
extern uint8_t __ttb0_l2_periph;

#define PERI_TABS_SIZE 0x10

#define TAB_SIZE 0x1000

#define PAGE_4K 0x1000

#define update_l2_item(t, va, pa) \
    do { \
        uint64_t *tab = t; \
        *(tab + (((va) >> 21) & 0x1ff)) = ((pa) & (~0x1fffff)) | (TT_S1_ATTR_BLOCK | (2 << TT_S1_ATTR_MATTR_LSB) | TT_S1_ATTR_NS | TT_S1_ATTR_AP_RW_PL1 | TT_S1_ATTR_AF | TT_S1_ATTR_nG); \
    } while (0)

#define update_l3_item(t, va, pa) \
    do { \
        uint64_t *tab = t; \
        *(tab + (((va) >> 12) & 0x1ff)) = ((pa) & (~0xfff)) | (TT_S1_ATTR_PAGE | (2 << TT_S1_ATTR_MATTR_LSB) | TT_S1_ATTR_NS | TT_S1_ATTR_AP_RW_PL1 | TT_S1_ATTR_AF | TT_S1_ATTR_nG); \
    } while (0)

#define update_l2_table(t, va, n) \
    do { \
        uint64_t *tab = t; printf("L2 %lx\n",((uint64_t) virt_to_phys(n) & (~0xfff)));\ 
        *(tab + (((va) >> 21) & 0x1ff)) = ((uint64_t) virt_to_phys(n) & (~0xfff)) | TT_S1_ATTR_TABLE; \
    } while (0)

#define get_l2_item(t, va) *((uint64_t *)(t) + (((va) >> 21) & 0x1ff))

#if 0
void mmap_dev_l3(uint64_t va, uint64_t pa, size_t size)
{
    uint64_t *tab1 = &__ttb0_l1;
    uint64_t *tab2 = &__ttb0_l2_periph;
    uint64_t *tab3 = &__ttb0_l3_periph_tabs;
    uint64_t i;

    // check item of table level 1
    if (0 == *(tab1 + ((va >> 30) & 0xf))) {
	// level 2
	update_l2_item(tab2, va, pa);
        __asm__ __volatile__("dsb ish");

	// level 1
        *(tab1 + ((va >> 30) & 0xf)) = (uint64_t) virt_to_phys(tab2) | TT_S1_ATTR_TABLE;
        __asm__ __volatile__("dsb ish");
    } else {
        // get level 2, notice tab2 is get from tab1, so tab2 is physical address
        tab2 = *(tab1 + ((va >> 30) & 0xf)) & (~0xfff);
	if (0 == get_l2_item(tab2, va)) {
            memset(tab3, 0, TAB_SIZE);printf("clear tab3\n");
	}
        
	// level 3
	printf("sttart add %lx, %lx\n", va, pa);
	for (i = 0; i < size; i += PAGE_4K) {
	    update_l3_item(tab3, va + i, pa + i);
	}printf("sttart add %lx, %lx, %lx\n", va + i, pa + i, tab3);
	__asm__ __volatile__("dsb ish");
	
	// level 2, notice tab2 is get from tab1, so tab2 is physical address
	tab2 = *(tab1 + ((va >> 30) & 0xf)) & (~0xfff);
	// we could access physcial address beacause of shadow map
	update_l2_table(tab2, va, tab3);
	__asm__ __volatile__("dsb ish");
    }

    // flush tlb
    __asm__ __volatile__("tlbi VMALLE1");
}
#endif

void mmap_dev(uint64_t va, uint64_t pa, size_t size)
{
    uint64_t *tab1 = &__ttb0_l1;
    uint64_t *tab2 = &__ttb0_l2_periph;

    // check item of table level 1
    if (0 == *(tab1 + ((va >> 30) & 0xf))) {
	// level 2
	update_l2_item(tab2, va, pa);
        __asm__ __volatile__("dsb ish");

	// level 1
        *(tab1 + ((va >> 30) & 0xf)) = (uint64_t) virt_to_phys(tab2) | TT_S1_ATTR_TABLE;
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

