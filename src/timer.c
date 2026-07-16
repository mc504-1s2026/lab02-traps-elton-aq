#include <arch/timer.h>
#include <arch/csr.h>
#include <kernel/printf.h>

/* guarda o valor de "time" em que o alarme deve disparar */
static volatile u64 alarm_deadline = 0;
/* indica se tem algum alarme agendado esperando disparar */
static volatile int alarm_armed = 0;

u64 timer_read()
{
	/* CSR de tempo, incrementado a 10MHz desde o boot */
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	/* agenda a primeira interrupção de timer daqui a 1 segundo */
	csr_write(CSR_STIMECMP, timer_read() + TIMER_FREQ);
	/* habilita a interrupção de timer no sie */
	csr_set(CSR_SIE, CSR_SIE_STIE);
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	/* calcula em quanto tempo do alarme */
	alarm_deadline = timer_read() + secs * TIMER_FREQ;
	alarm_armed = 1;
}

void timer_irq()
{
	/* se tem alarme armado e já passou do prazo, dispara e desarma */
	if (alarm_armed && timer_read() >= alarm_deadline) {
		alarm_armed = 0;
		print("alarm\n");
	}

	/* reagenda o próximo tick pra continuar recebendo interrupções */
	csr_write(CSR_STIMECMP, timer_read() + TIMER_FREQ);
}
