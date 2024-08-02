#include <stdio.h>

#include <libfdt.h>

#include "v8_mmu.h"
#include "gicv3.h"
#include "gicv3_gicc.h"

extern uint8_t __cs3_peripherals;

static uint64_t gicd_offset = 0;
static uint64_t gicr_offset = 0;

static void dump_dtb(void *fdt)
{
    int offset, nextoffset = 0;
    uint32_t tag;
    const void *prop;
    int err;
    int len;
    const char *propname;

    while (1) {
        offset = nextoffset;
        tag = fdt_next_tag(fdt, offset, &nextoffset);
        switch (tag) {
        case FDT_BEGIN_NODE:
            printf("begin %s\n",fdt_get_name(fdt, offset, &len));
            break;
        case FDT_PROP:
            prop = fdt_getprop_by_offset(fdt, offset, &propname, &err);
            printf("prop %s\n",propname);
            if (0 == strcmp(propname, "bootargs")) {
                printf("bootargs: %s\n", (char *) fdt_getprop(fdt, offset, "bootargs", &len));
            }
            break;
    case FDT_END:
        printf("dump end\n");
        return;
        break;
        default:
            break;
        }
    }
}

static void init_gic(void *device_tree)
{
    uint64_t addr;
    int node = 0;
    int depth = 0;

    for (;;) {
        node = fdt_next_node(device_tree, node, &depth);
        if (node <= 0 || depth < 0)
            break;

        if (fdt_getprop(device_tree, node, "interrupt-controller", NULL)) {
            int len = 0;

            const uint64_t *reg = fdt_getprop(device_tree, node, "reg", &len);

            /* two registers (GICC and GICD), each of which contains
             * two parts (an address and a size), each of which is a 64-bit
             * value (8 bytes), so we expect a length of 2 * 2 * 8 = 32.
             */
            if (reg == NULL || len < 32) {
                printf("bad 'reg' property: %p %d\n", reg, len);
                continue;
            }

#define MASK_2M (2*1024*1024 - 1)

        addr = fdt64_to_cpu(reg[0]);
        gicd_offset = addr & MASK_2M;
        mmap_dev((uint64_t) &__cs3_peripherals + gicd_offset, addr, fdt64_to_cpu(reg[1]));

        addr = fdt64_to_cpu(reg[2]);
        gicr_offset = addr & MASK_2M;
        mmap_dev((uint64_t) &__cs3_peripherals + gicr_offset, addr, fdt64_to_cpu(reg[3]));

        printf("gicd_base = %lx, %lx. gicr_base = %lx, %lx.\n", fdt64_to_cpu(reg[0]), fdt64_to_cpu(reg[1]), fdt64_to_cpu(reg[2]), fdt64_to_cpu(reg[3]));
            return;
        }
    }

    printf("gic not found!\n");
}

void setup_gic(void *dtb)
{
    dump_dtb(dtb);

    // get pa from dtb
    init_gic(dtb);

    printf("gicd %p, gicr %p\n", &__cs3_peripherals + gicd_offset, &__cs3_peripherals + gicr_offset);
    init_gicd_base((uint64_t) &__cs3_peripherals + gicd_offset);
    init_gicr_base((uint64_t) &__cs3_peripherals + gicr_offset);

    // Enable system register access to GIC
    uint64_t sre = getICC_SRE_EL1();
    sre |= sreDFB;
    setICC_SRE_EL1(sre);

    // Set interrupt priority mask to allow all priorities
    setICC_PMR(0xff);

    // Enable Group 1 interrupts
    setICC_CTLR_EL1(ctlrCBPR_EL1S);
    setICC_IGRPEN1_EL1(igrpEnable);

    // Wake up the redistributor
    WakeupGICR(GICR_INDEX);

    // Enable the distributor
    EnableGICD(gicdctlr_EnableGrp1NS);
}

