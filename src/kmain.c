#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h>

extern int _hartid[];

/* interpreta e executa um comando digitado no shell */
static void run_command(char *line)
{
	if (strncmp(line, "echo ", 5) == 0) {
		serial_puts(line + 5);
		serial_putc('\n');
	} else if (strcmp(line, "uptime") == 0) {
		/* tempo desde o boot em segundos = time / TIMER_FREQ */
		u64 secs = timer_read() / TIMER_FREQ;
		char msg[32];
		snprintf(msg, sizeof(msg), "%lus\n", secs);
		serial_puts(msg);
	} else if (strncmp(line, "alarm ", 6) == 0) {
		u64 secs = strtou64(line + 6, 10);
		timer_set_alarm(secs);
	}
}

void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	/* só depois de configurar tudo é que ligamos as interrupções de fato */
	hart_irq_enable();

	char line[256];
	size_t linelen = 0;

	serial_puts("> ");

	/* loop principal, fica esperando bytes chegarem pela serial
	 * (via interrupção) e vai montando a linha digitada até o Enter */
	while (1) {
		char chunk[256];
		size_t got = serial_read(chunk);

		for (size_t i = 0; i < got; i++) {
			char c = chunk[i];

			if (c == '\r') {
				/* Enter: fecha a linha, executa o comando e mostra o prompt de novo */
				serial_putc('\r');
				serial_putc('\n');
				line[linelen] = '\0';
				if (linelen > 0) {
					run_command(line);
				}
				linelen = 0;
				serial_puts("> ");
			} else if (c == 127 || c == '\b') {
				/* backspace: apaga o último caractere digitado */
				if (linelen > 0) {
					linelen--;
					serial_putc('\b');
					serial_putc(' ');
					serial_putc('\b');
				}
			} else if (linelen < sizeof(line) - 1) {
				/* caractere normal: guarda na linha e ecoa de volta */
				line[linelen++] = c;
				serial_putc(c);
			}
		}
	}
}
