MEMORY {
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 8K
    RAM (rwx) : ORIGIN = 0x10000000 + 2K, LENGTH = 4K - 2K /* Skip first 2K for the OSD */
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
    . = ALIGN(4);
    _data_start = .;
    *(.sdata)
    *(.sdata.*)
    *(.data)
    *(.data.*)
    _data_end = .;
  } > RAM AT> ROM
  .bss :
  {
    . = ALIGN(4);
    _bss_start = .;
    *(.sbss)
    *(.sbss.*)
    *(.bss)
    *(.bss.*)
    _bss_end = .;
  } > RAM
  _idata_start = LOADADDR(.data);
}
