OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);       /* 确保 trampoline 对齐到 4KB 页边界 */
    _trampoline = .;         /* 定义 trampoline 开始位置 */
    *(trampsec)              /* 放入 trampoline.S 中的代码 */
    . = ALIGN(0x1000);       /* 再次对齐，确保它独占一页 */
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
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
    PROVIDE(__global_pointer$ = .);
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