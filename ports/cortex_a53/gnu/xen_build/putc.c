
#include <stdint.h>
#include <stddef.h>

#define CONSOLEIO_write 0

void HYPERVISOR_console_io(int no, size_t size, uint8_t *str);

int console_putc(unsigned char c)
{
	HYPERVISOR_console_io(CONSOLEIO_write, 1, &c);
	return 1;
}

