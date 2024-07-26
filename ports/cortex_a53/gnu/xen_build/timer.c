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

#define write_sysreg(reg, val) __asm__ volatile ("msr " #reg ", %0" : : "r"(val))
#define read_sysreg(reg) ({ uint64_t val; __asm__ volatile ("mrs %0, " #reg : "=r"(val)); val; })

#define CONSOLEIO_write 0

void irqHandler(void)
{
  unsigned int ID;

  ID = getICC_IAR1(); // readIntAck();
    HYPERVISOR_console_io(CONSOLEIO_write, 8, "time irq\n");
  // Check for reserved IDs
  if ((1020 <= ID) && (ID <= 1023))
  {
      //printf("irqHandler() - Reserved INTID %d\n\n", ID);
      return;
  }

  switch(ID)
  {
    case 27:
      HYPERVISOR_console_io(CONSOLEIO_write, 8, "vvvv irq\n");
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

// Virtual Timer registers
#define CNTV_CTL_EL0    0x000
#define CNTV_TVAL_EL0   0x008
#define CNTV_CVAL_EL0   0x010
#define CNTVCT_EL0      0x018

// System register access macros
#define read_sysreg(reg) ({ uint64_t val; asm volatile("mrs %0, " #reg : "=r"(val)); val; })
#define write_sysreg(reg, val) ({ asm volatile("msr " #reg ", %0" :: "r"(val)); })

// ICC register access (CPU Interface)
#define ICC_CTLR_EL1            0xC12
#define ICC_PMR_EL1             0xC13
#define ICC_IAR1_EL1            0xC0C
#define ICC_EOIR1_EL1           0xC10
#define ICC_SRE_EL1             0xC15

// Virtual Timer interrupt ID for GIC
#define VIRTUAL_TIMER_IRQ       27

void setup_gic(void)
{
    // Enable system register access to GIC
    uint64_t sre = read_sysreg(ICC_SRE_EL1);
    sre |= 1;
    write_sysreg(ICC_SRE_EL1, sre);

    // Set interrupt priority mask to allow all priorities
    write_sysreg(ICC_PMR_EL1, 0xFF);

    // Enable Group 1 interrupts
    write_sysreg(ICC_CTLR_EL1, 1);
}

// 设置计时器初始值
void configure_vtimer(uint64_t ms)
{
    uint64_t freq = read_sysreg(CNTFRQ_EL0); // 获取计时器频率
    uint64_t interval = (freq / 1000) * ms; // 将毫秒转换为计时器周期

    write_sysreg(CNTV_TVAL_EL0, interval);
    uint64_t ctl = read_sysreg(CNTV_CTL_EL0);
    write_sysreg(CNTV_CTL_EL0, ctl | 1); // 设置使能位
}

// 处理计时器中断
void handle_vtimer_interrupt(void)
{
    // 在这里处理计时器中断，例如更新系统时间、执行调度等
    // 清除计时器中断状态
    uint64_t ctl = read_sysreg(CNTV_CTL_EL0);
    write_sysreg(CNTV_CTL_EL0, ctl & ~2); // 清除中断位

    configure_vtimer(10); // 10ms
}

#define ICC_PMR_EL1     "S3_0_C4_C6_0"
#define ICC_CTLR_EL1    "S3_0_C12_C12_4"
#define ICC_IAR1_EL1    "S3_0_C12_C12_0"
#define ICC_EOIR1_EL1   "S3_0_C12_C12_1"
#define ICC_SRE_EL1     "S3_0_C12_C12_5"

#define GICD_BASE       0x3001000// GIC Distributor base address (example)
#define GICR_BASE       0x3020000 // GIC Redistributor base address (example)
#define GICD_CTLR       (GICD_BASE + 0x0000)
#define GICD_ISENABLER  (GICD_BASE + 0x0100)
#define GICR_WAKER      (GICR_BASE + 0x0014)
#define GICR_CTLR       (GICR_BASE + 0x0000)
#define GICR_ISENABLER  (GICR_BASE + 0x0100)

// Initialize Timer 0 and Interrupt Controller
void init_timer(void)
{
    setup_gic();

    // Wake up the redistributor
    *(volatile uint32_t *)(GICR_WAKER) &= ~(1 << 1);
    while (*(volatile uint32_t *)(GICR_WAKER) & (1 << 2));

    // Enable the redistributor
    *(volatile uint32_t *)(GICR_CTLR) = 1;//gicdctlr_EnableGrp1NS;

    // Enable the distributor
    *(volatile uint32_t *)(GICD_CTLR) = gicdctlr_EnableGrp1NS;

    // Enable the specific interrupt ID for the virtual timer (27 in this example)
    *(volatile uint32_t *)(GICR_ISENABLER) = (1 << (27 % 32));
    *(volatile uint32_t *)(GICD_ISENABLER) = (1 << (27 % 32));

    configure_vtimer(100); // 设置计时器值为1000000（假设计时器频率为1 MHz，即1秒）

    __asm__("msr daifclr, #2"); // Enable IRQs
    setICC_IGRPEN1_EL1(igrpEnable);
#if 0
    while (1) {
	    uint32_t intid, iar;

        asm volatile("wfi");
        // 轮询中断状?
        iar = read_sysreg(ICC_IAR1_EL1);
        intid = iar & 0xFFFFFF;
        char buf[0x100];int n;// = { 'i', 'n', 't', ' ', '9', '\0' };
        n = snprintf(buf, 100, "intid %d\n", intid);
        HYPERVISOR_console_io(CONSOLEIO_write, n, buf);
        if (intid == VIRTUAL_TIMER_IRQ) {
            HYPERVISOR_console_io(CONSOLEIO_write, 8, "xxxxxxxxxxxxxxx");
        }

        uint64_t ctl = read_sysreg(CNTV_CTL_EL0);
        if (ctl & 2) { // 检查中断位是否设置
            handle_vtimer_interrupt();
            HYPERVISOR_console_io(CONSOLEIO_write, 8, "tt-tttttt\n");
            configure_vtimer(10000000); // 重新设置计时器值
        } else {
            //HYPERVISOR_console_io(CONSOLEIO_write, 1, ".\n");
        }
    }
#endif
}


