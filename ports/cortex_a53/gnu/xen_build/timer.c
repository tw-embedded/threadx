/* Bare-metal example for Armv8-A FVP Base model */

/* Timer and interrupts */

/* Copyright (c) 2016-2018 Arm Limited (or its affiliates). All rights reserved. */
/* Use, modification and redistribution of this file is subject to your possession of a     */
/* valid End User License Agreement for the Arm Product of which these examples are part of */
/* and your compliance with all applicable terms and conditions of such licence agreement.  */

#include <stdio.h>

#include "gicv3.h"
#include "gicv3_gicc.h"

void   _tx_timer_interrupt(void);



void irqHandler(void)
{
  unsigned int ID;

  ID = getICC_IAR1(); // readIntAck();

  // Check for reserved IDs
  if ((1020 <= ID) && (ID <= 1023))
  {
      //printf("irqHandler() - Reserved INTID %d\n\n", ID);
      return;
  }

  switch(ID)
  {
    case 34:
      // Dual-Timer 0 (SP804)
      //printf("irqHandler() - External timer interrupt\n\n");

      /* Call ThreadX timer interrupt processing.  */
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

// --------------------------------------------------------

// Not actually used in this example, but provided for completeness

void fiqHandler(void)
{
  unsigned int ID;
  unsigned int aliased = 0;

  ID = getICC_IAR0(); // readIntAck();
  //printf("fiqHandler() - Read %d from IAR0\n", ID);

  // Check for reserved IDs
  if ((1020 <= ID) && (ID <= 1023))
  {
    //printf("fiqHandler() - Reserved INTID %d\n\n", ID);
    ID = getICC_IAR1(); // readAliasedIntAck();
    //printf("fiqHandler() - Read %d from AIAR\n", ID);
    aliased = 1;

    // If still spurious then simply return
    if ((1020 <= ID) && (ID <= 1023))
        return;
  }

  switch(ID)
  {
    case 34:
      // Dual-Timer 0 (SP804)
      //printf("fiqHandler() - External timer interrupt\n\n");
      //clearTimerIrq();
      break;

    default:
      // Unexpected ID value
while (1);
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

static void ArmWriteCntvCval(uint64_t value)
{
    __asm__ __volatile__("ldr x0, %0\n\t"
                         "msr cntv_cval_el0, x0\n\t"
                         :
                         :"m"(value)
                         :"memory");
}

static void set_timer_period(uint64_t period)
{
    uint64_t tick;
    uint64_t count;

    //stopTimer();
    if (period) {
        // mTimerTicks = TimerPeriod in 1ms unit x Frequency.10^-3
        //             = TimerPeriod.10^-4 x Frequency.10^-3
        //             = (TimerPeriod x Frequency) x 10^-7
        tick = period * ArmReadCntFrq();
        tick /= 10000000U;

        // Get value of the current timer
        count = ArmReadCntvCt();
        // Set the interrupt in Current Time + mTimerTick
        ArmWriteCntvCtl(count + tick);

        //startTimer();
    }
}

// Initialize Timer 0 and Interrupt Controller
void init_timer(void)
{
    uint64_t reg;

    // disable timer
    reg = ArmReadCntvCtl();
    reg |= ARM_ARCH_TIMER_IMASK;
    reg &= ~ARM_ARCH_TIMER_ENABLE;
    ArmWriteCntvCtl(reg);

    // install interrupt handler

    // setup timer
    set_timer_period(100000);

    // enable timer
    reg = ARM_ARCH_TIMER_ENABLE;
    ArmWriteCntvCtl(reg);

    // Enable interrupts
    __asm("MSR DAIFClr, #0xF");
    setICC_IGRPEN1_EL1(igrpEnable);

    // Route INTID 27 to 0.0.0.0 (this core)
    SetSPIRoute(27, 0, gicdirouter_ModeSpecific);
    // Set INTID 27 to priority to 0
    SetSPIPriority(27, 0);
    // Set INTID 27 as level-sensitive
    ConfigureSPI(27, gicdicfgr_Edge);
    // Enable INTID 27
    EnableSPI(27);
}

static void reenable_timer(void)
{
    uint64_t TimerCtrlReg;

    TimerCtrlReg  = ArmReadCntvCtl ();
    TimerCtrlReg |= ARM_ARCH_TIMER_ENABLE;

    //
    // When running under Xen, we need to unmask the interrupt on the timer side
    // as Xen will mask it when servicing the interrupt at the hypervisor level
    // and delivering the virtual timer interrupt to the guest. Otherwise, the
    // interrupt will fire again, trapping into the hypervisor again, etc. etc.
    //
    TimerCtrlReg &= ~ARM_ARCH_TIMER_IMASK;
    ArmWriteCntvCtl (TimerCtrlReg);
}

void irq()
{
    uint64_t count, compare;

    // eoi

    // check interrupt source
    if (ArmReadCntvCtl() & ARM_ARCH_TIMER_ISTATUS) {
        // Get current counter value
        count = ArmReadCntvCt();
        // Get the counter value to compare with
        compare = ArmReadCntvCval();
        // Set next compare value
        ArmWriteCntvCval(compare);
        reenable_timer();
        __asm__ __volatile__("isb\n\t");
    }
}

