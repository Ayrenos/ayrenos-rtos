ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* Start stack at the top of RAM and grows downward */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Mark bottom of the 1 KB space planned for the stack */
_stack_limit = _estack - 1024;

SECTIONS
{
    /* Output section for vector table in flash memory */
    .isr_vector :
    {
        /* Collect vector tables from all object files and keep even if unused */
        KEEP(*(.isr_vector))
    } > FLASH

    /* Output section for code and read-only data in flash memory */
    .text :
    {
        /* Collect program instruction from all object files */
        *(.text*)

        /* Collect read-only data, such as string literals and constants */
        *(.rodata*)

        /* Padding if needed so section ends on 4-byte boundary */
        . = ALIGN(4);
    } > FLASH

    /* Create section for stack-unwinding details in flash memory */
    .ARM.extab :
    {
        /* Collect these tables from all object files */
        *(.ARM.extab*)
    } > FLASH

    /* Create index for finding unwinding info in flash memory */
    .ARM.exidx :
    {
        /* Collect index entries from all object files */
        *(.ARM.exidx*)
    } > FLASH

    /* Create section for initialized writeable variables */
    .data :
    {
        . = ALIGN(4);

        /* Record start of data in RAM */
        _sdata = .;

        /* Collect initialized data from all object files */
        *(.data*)

        . = ALIGN(4);

        /* Record address after data in RAM */
        _edata = .;
    } > RAM AT > FLASH /* RAM at runtime; intial values stored in Flash */

    /* Record where initial values begin in Flash */
    _sidata = LOADADDR(.data);

    /* Allocate RAM without storing initial contents in firmware image */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);

        /* Record where zero-initialized data begins */
        _sbss = .;

        /* Collect zero-initialized variables from all object files */
        *(.bss*)

        /* Include unitialized globals represented as COMMON symbols */
        *(COMMON)

        . = ALIGN(4);

        /* Record address just after this area */
        _ebss = .;
    } > RAM

    /* Fail build if area extends into main stack budget */
    ASSERT(_ebss <= _stack_limit, "RAM DATA OVERLAPS RESERVED MAIN STACK")
}