
#include "gicv3.h"
#include "gicv3_gicc.h"

#define GICR_INDEX 0

#define SCALE 100

// 1 tick is 10ms
#define TICK (10 * SCALE)

#define CONSOLEIO_write 0

// Virtual Timer interrupt ID for GIC
#define VIRTUAL_TIMER_IRQ 27

//
// Accessors for the architected generic timer registers
//
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

static uint64_t ArmReadCntFrq(void)
{
    uint64_t value;

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

static void setup_gic(void)
{
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

static void configure_vtimer(uint64_t ms)
{
    uint64_t freq = ArmReadCntFrq();
    uint64_t interval = (freq / 1000) * ms;

    ArmWriteCntvTval(interval);

    uint64_t ctl = ArmReadCntvCtl();
    ArmWriteCntvCtl(ctl | ARM_ARCH_TIMER_ENABLE); // enable
}

static void handle_vtimer_interrupt(void)
{
    uint64_t ctl = ArmReadCntvCtl();
    ArmWriteCntvCtl(ctl & ~ARM_ARCH_TIMER_IMASK); // clear interrupt

    configure_vtimer(TICK);
}

void _tx_timer_interrupt(void);

void irqHandler(void)
{
  unsigned int ID;

  ID = getICC_IAR1(); // readIntAck();
    //HYPERVISOR_console_io(CONSOLEIO_write, 8, "time irq\n");
  // Check for reserved IDs
  if ((1020 <= ID) && (ID <= 1023)) {
      //printf("irqHandler() - Reserved INTID %d\n\n", ID);
      return;
  }

  switch (ID) {
    case VIRTUAL_TIMER_IRQ:
      //HYPERVISOR_console_io(CONSOLEIO_write, 8, "vvvv irq\n");
      handle_vtimer_interrupt();
      _tx_timer_interrupt();
      break;

    default:
      // Unexpected ID value
      //printf("irqHandler() - Unexpected INTID %d\n\n", ID);
      break;
  }

  // Write the End of Interrupt register to tell the GIC
  // we've finished handling the interrupt
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

// Initialize Timer 0 and Interrupt Controller
void init_timer(void)
{
    setup_gic();

    // Enable the specific interrupt ID for the virtual timer 
    EnableSPI(VIRTUAL_TIMER_IRQ);
    EnablePrivateInt(GICR_INDEX, VIRTUAL_TIMER_IRQ);

    configure_vtimer(TICK);

    __asm__("msr daifclr, #2"); // Enable IRQs

#if 0
    while (1) {
        uint32_t intid, iar;

        asm volatile("wfi");

#define read_sysreg(reg) ({ uint64_t val; __asm__ volatile ("mrs %0, " #reg : "=r"(val)); val; })
#define write_sysreg(reg, val) __asm__ volatile ("msr " #reg ", %0" : : "r"(val))

        iar = read_sysreg(ICC_IAR1_EL1);
        intid = iar & 0xFFFFFF;
        char buf[0x100];int n;
        n = snprintf(buf, 100, "intid %d\n", intid);
        HYPERVISOR_console_io(CONSOLEIO_write, n, buf);
        if (intid == VIRTUAL_TIMER_IRQ) {
            HYPERVISOR_console_io(CONSOLEIO_write, 8, "xxxxxxxxxxxxxxx");
        }

        uint64_t ctl = read_sysreg(CNTV_CTL_EL0);
        if (ctl & 2) {
            handle_vtimer_interrupt();
            HYPERVISOR_console_io(CONSOLEIO_write, 8, "tt-tttttt\n");
            configure_vtimer(10000000);
        } else {
            //HYPERVISOR_console_io(CONSOLEIO_write, 1, ".\n");
        }
    }
#endif
}

