#include <kernel/serial.h>
#include <kernel/mm.h>
#include <arch/io.h>
#include <arch/csr.h>
#include <arch/plic.h>
#include <arch/spinlock.h>

#define RX_BUF_SIZE 256

static u8 *uart_base;

static struct {
	char rxbuf[RX_BUF_SIZE];
	size_t rxlen;
	struct spinlock lock;
} uart;

static inline void uart_write(u64 reg, u8 val)
{
	iowrite8(val, uart_base + reg);
}

static inline u8 uart_read(u64 reg)
{
	return ioread8(uart_base + reg);
}

void serial_init()
{
	uart_base = (u8 *)pa_to_va((phys_addr_t)(u64)SERIAL_BASE);
	uart.rxlen = 0;
	spin_init(&uart.lock);

	uart_write(SERIAL_IER, 0x00);
	/* liga o DLAB pra poder configurar o divisor da taxa de transmissão */
	uart_write(SERIAL_LCR, 0x80);
	uart_write(SERIAL_RBR, 0x03);
	uart_write(SERIAL_IER, 0x00);
	/* desliga o DLAB e configura 8 bits, sem paridade, 1 stop bit */
	uart_write(SERIAL_LCR, 0x03);
	/* habilita as FIFOs de recepção/transmissão e limpa ambas */
	uart_write(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE
			 | SERIAL_FCR_RX_FIFO_CLEAR
			 | SERIAL_FCR_TX_FIFO_CLEAR);
	uart_write(SERIAL_MCR, 0x00);
}

void serial_irq_enable()
{
	/* avisa a UART pra gerar interrupção */
	uart_write(SERIAL_IER, SERIAL_IER_ERBFI);

	/* configura o PLIC */
	plic_irq_set_priority((u32)IRQ_SERIAL, 1);
	plic_hart_enable_irq(0, (u32)IRQ_SERIAL);
	plic_hart_set_threshold(0, 0);

	/* habilita interrupções externas no sie */
	csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
	uart_write(SERIAL_IER, 0x00);
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	/* protege o buffer compartilhado com o serial_read() */
	u64 flags = spin_lock_irqsave(&uart.lock);
	while (uart_read(SERIAL_LSR) & SERIAL_LSR_DTR) {
		u8 byte = uart_read(SERIAL_RBR);
		if (uart.rxlen < RX_BUF_SIZE) {
			uart.rxbuf[uart.rxlen++] = (char)byte;
		}
	}
	spin_unlock_irqrestore(&uart.lock, flags);
}

size_t serial_read(char *buf)
{
	/* copia tudo que tiver no buffer interno e zera ele*/
	u64 flags = spin_lock_irqsave(&uart.lock);
	size_t count = uart.rxlen;
	for (size_t i = 0; i < count; i++) {
		buf[i] = uart.rxbuf[i];
	}
	uart.rxlen = 0;
	spin_unlock_irqrestore(&uart.lock, flags);
	return count;
}

void serial_putc(char c)
{
	/* espera o registrador de transmissão ficar livre antes de escrever */
	while (!(uart_read(SERIAL_LSR) & SERIAL_LSR_THRE))
		;
	uart_write(SERIAL_THR, (u8)c);
}

void serial_puts(char *str)
{
	while (*str) {
		serial_putc(*str);
		str++;
	}
}
