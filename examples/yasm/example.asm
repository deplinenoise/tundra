
	BITS 32
	GLOBAL _foo
	EXTERN _puts

	SECTION .rodata

fmt:	db "Hello world from asm", 0

	SECTION .text

_foo:	
	sub		esp, 8
	push	fmt
	call	_puts
	add		esp, 12
	ret
	

