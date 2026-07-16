#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <arch/csr.h>
#include <arch/plic.h>
#include <arch/timer.h>
#include <kernel/serial.h>

/* definido em src/trap_entry.S */
extern void trap_entry();

void trap_setup()
{
	/* aponta o stvec pro nosso handler de trap em assembly */
	csr_write(CSR_STVEC, (u64)trap_entry);
}

void hart_irq_enable()
{
	/* liga o bit SIE em sstatus, habilitando interrupções nesse hart */
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void hart_irq_disable()
{
	/* desliga o bit SIE, desabilitando interrupções */
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	/* lê o estado atual de SIE e já desliga, tudo em uma instrução atômica */
	return csr_read_clear(CSR_SSTATUS, CSR_SSTATUS_SIE) & CSR_SSTATUS_SIE;
}

void hart_irq_restore(u64 flags)
{
	/* volta o estado de SIE que foi salvo por hart_irq_save() */
	csr_set(CSR_SSTATUS, flags & CSR_SSTATUS_SIE);
}

void handle_irq()
{
	u64 scause = csr_read(CSR_SCAUSE);

	if (scause == TRAP_TIMER_IRQ) {
		timer_irq();
	} else if (scause == TRAP_EXTERNAL_IRQ) {
		/* interrupção externa (ex: serial) precisa do handshake com o PLIC:
		 * primeiro reivindicamos (claim) qual IRQ aconteceu */
		u32 irq = plic_hart_claim_irq(0);
		if (irq == IRQ_SERIAL) {
			serial_irq();
		}
		/* depois avisamos o PLIC que terminamos de tratar */
		if (irq) {
			plic_hart_complete_irq(0, irq);
		}
	}
}

void handle_exception()
{
	u64 scause = csr_read(CSR_SCAUSE);
	u64 stval = csr_read(CSR_STVAL);
	u64 sepc = csr_read(CSR_SEPC);

	/* não tratamos exceções específicas neste lab, então só reportamos
	 * o que aconteceu e paramos o kernel */
	panic("unhandled exception: scause=%lx stval=%lx sepc=%lx\n",
	      scause, stval, sepc);
}

void handle_trap()
{
	u64 scause = csr_read(CSR_SCAUSE);

	/* o bit mais significativo de scause diz se é interrupção (1) ou
	 * exceção síncrona (0) */
	if (scause & TRAP_IRQ_BIT) {
		handle_irq();
	} else {
		handle_exception();
	}
}
