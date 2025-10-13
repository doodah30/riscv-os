OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text .text.*)
    PROVIDE(etext = .);
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    __bss_start = .;
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    . = ALIGN(16);
    __bss_end = .;
  }

  /* --- 新增内容 --- */
  . = ALIGN(4096);
  
  .stack : {
    stack0 = .; /* 定义 stack0 符号的地址 */
    . += 4096 * 4; /* NCPU=4，为栈保留 4*4096 字节的空间 */
  }

  PROVIDE(end = .);
}