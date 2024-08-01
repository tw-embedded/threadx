#include <stdio.h>

#include <libfdt.h>

#include "gicv3.h"
#include "gicv3_gicc.h"

extern uint8_t __attribute__((section(".gicd"))) gicd;

static void init_gic(void *device_tree)
{
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

            mmap_dev(&gicd, fdt64_to_cpu(reg[0]));
	    //mmap_dev(0x4020000, fdt64_to_cpu(reg[2]));
            printf("gicd_base = %p, gicc_base = %p\n", fdt64_to_cpu(reg[0]), fdt64_to_cpu(reg[2]));
            return;
        }
    }

    printf("gic not found!\n");
}

void setup_gic(void *dtb)
{
    // get pa from dtb
    init_gic(dtb);

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

