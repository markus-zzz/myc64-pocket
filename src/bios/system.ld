MEMORY {
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 8K
    RAM (rwx) : ORIGIN = 0x10000000 + 2K, LENGTH = 4K - 2K
}

SECTIONS {
  __stacktop = ORIGIN(RAM) + LENGTH(RAM);

  . = ORIGIN(ROM);
  .text :
  {
    start*(.text)
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
  } > ROM

  . = ORIGIN(RAM);
  .data :
  {
    *(.data)
  } > RAM
  .bss :
  {
    *(.data)
  } > RAM
}
