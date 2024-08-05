#include <stdio.h>

#include <libfdt.h>

#include "gicv3.h"
#include "gicv3_gicc.h"

// 1 tick is 1s
#define TICK (1000)

// Virtual Timer interrupt ID for GIC
#define VIRTUAL_TIMER_IRQ 27

// Accessors for the architected generic timer registers
#define ARM_ARCH_TIMER_ENABLE   (1 << 0)
#define ARM_ARCH_TIMER_IMASK    (1 << 1)
#define ARM_ARCH_TIMER_ISTATUS  (1 << 2)

static uint64_t ArmReadCntvCtl(void)
{
    uint64_t value;

    __asm__ __volatile__("mrs x0, cntv_ctl_el0\n\t"
                         "str x0, %0\n\t"
                         :"=m"(value)
                         :
                         :"memory");
    return value;
}

static void ArmWriteCntvCtl(uint64_t value)
{
    // Write to CNTV_CTL (Virtual Timer Control Register)
    __asm__ __volatile__("ldr x0, %0\n\t"
                         "msr cntv_ctl_el0, x0\n\t"
                         :
                         :"m"(value)
                         :"memory");
}

static uint64_t clock_frequency = 0;

static uint64_t ArmReadCntFrq(void)
{
    uint64_t value;

    if (clock_frequency) {
        return clock_frequency;
    }

    __asm__ __volatile__("mrs x0, cntfrq_el0\n\t"
                         "str x0, %0\n\t"
                         :"=m"(value)
                         :
                         :"memory");
    return value;
}

static uint64_t ArmReadCntvCt(void)
{
    uint64_t value;

    __asm__ __volatile__("mrs x0, cntvct_el0\n\t"
                         "str x0, %0\n\t"
                         :"=m"(value)
                         :
                         :"memory");
    return value;
}

static void ArmWriteCntvTval(uint64_t value)
{
    __asm__ __volatile__("ldr x0, %0\n\t"
                         "msr cntv_tval_el0, x0\n\t"
                         :
                         :"m"(value)
                         :"memory");
}

static void configure_vtimer(uint64_t ms)
{
    uint64_t freq = ArmReadCntFrq();
    uint64_t interval = (freq / 1000) * ms;

    ArmWriteCntvTval(interval);
}

static void enable_vtimer(void)
{
    uint64_t ctl = ArmReadCntvCtl();
    ArmWriteCntvCtl(ctl | ARM_ARCH_TIMER_ENABLE); // enable
}

static void handle_vtimer_interrupt(void)
{
    uint64_t ctl = ArmReadCntvCtl();
    configure_vtimer(TICK);
    ArmWriteCntvCtl(ctl & ~ARM_ARCH_TIMER_IMASK); // clear interrupt
}

void _tx_timer_interrupt(void);

void irqHandler(void)
{
  unsigned int ID;

  ID = getICC_IAR1(); // readIntAck();

  // Check for reserved IDs
  if ((1020 <= ID) && (ID <= 1023)) {
      printf("irqHandler() - Reserved INTID %d\n\n", ID);
      return;
  }

  switch (ID) {
    case VIRTUAL_TIMER_IRQ:
      handle_vtimer_interrupt();
      _tx_timer_interrupt();
      break;

    default:
      // Unexpected ID value
      printf("irqHandler() - Unexpected INTID %d\n\n", ID);
      break;
  }

  // write the End of Interrupt register to tell the GIC
  // finished handling the interrupt
  setICC_EOIR1(ID); // writeAliasedEOI(ID);
}

void fiqHandler(void)
{
  unsigned int ID;
  unsigned int aliased = 0;

  ID = getICC_IAR0(); // readIntAck();
  //printf("fiqHandler() - Read %d from IAR0\n", ID);

  // Check for reserved IDs
  if ((1020 <= ID) && (ID <= 1023)) {
    //printf("fiqHandler() - Reserved INTID %d\n\n", ID);
    ID = getICC_IAR1(); // readAliasedIntAck();
    //printf("fiqHandler() - Read %d from AIAR\n", ID);
    aliased = 1;

    // If still spurious then simply return
    if ((1020 <= ID) && (ID <= 1023))
        return;
  }

  switch(ID) {
    case 34:
      // Dual-Timer 0 (SP804)
      //printf("fiqHandler() - External timer interrupt\n\n");
      //clearTimerIrq();
      break;

    default:
      // Unexpected ID value
      //printf("fiqHandler() - Unexpected INTID %d\n\n", ID);
      break;
  }

  // Write the End of Interrupt register to tell the GIC
  // we've finished handling the interrupt
  // NOTE: If the ID was read from the Aliased IAR, then
  // the aliased EOI register must be used
  if (aliased == 0)
    setICC_EOIR0(ID); // writeEOI(ID);
  else
    setICC_EOIR1(ID); // writeAliasedEOI(ID);
}

static void setup_clock_freq(void *device_tree)
{
    int node = 0;
    int depth = 0;
    int len;
    const uint64_t *tmp;
    char *name;

    for (;;) {
        node = fdt_next_node(device_tree, node, &depth);
        if (node <= 0 || depth < 0)
            break;

        if (fdt_node_check_compatible(device_tree, node, "arm,armv8-timer")) {
            printf("timer found\n");
            printf("reg freq %lx\n", ArmReadCntFrq());
            tmp = fdt_getprop(device_tree, node, "clock-frequency", NULL);
            if (NULL != tmp) {
                clock_frequency = fdt64_to_cpu(*tmp);
                printf("new freq %lx\n", clock_frequency);
            }
            return;
        }
    }
}

static void wait_timer_interrupt_test(void)
{
    uint32_t intid, iar;

    while (1) {
        asm volatile("wfi");

#define read_sysreg(reg) ({ uint64_t val; __asm__ volatile ("mrs %0, " #reg : "=r"(val)); val; })
#define write_sysreg(reg, val) __asm__ volatile ("msr " #reg ", %0" : : "r"(val))

#define CONSOLEIO_write 0

        iar = read_sysreg(ICC_IAR1_EL1);
        intid = iar & 0xFFFFFF;

        printf("intid %d\n", intid);
        if (intid == VIRTUAL_TIMER_IRQ) {
            printf("virtual timer irq\n");
        }

        uint64_t ctl = read_sysreg(CNTV_CTL_EL0);
        if (ctl & 2) {
            handle_vtimer_interrupt();
            printf("x");
            configure_vtimer(10000000);
        } else {
            printf(".");
        }
    }
}

// initialize Timer 0 and Interrupt Controller
void init_timer(void *dtb)
{
    setup_clock_freq(dtb);

    // enable the specific interrupt ID for the virtual timer
    EnableSPI(VIRTUAL_TIMER_IRQ);
    //EnablePrivateInt(GICR_INDEX, VIRTUAL_TIMER_IRQ);

    configure_vtimer(TICK);

    enable_vtimer();

    __asm__("msr daifclr, #2"); // enable IRQs
}


