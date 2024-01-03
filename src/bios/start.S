.text
.globl _start
.p2align 2
.type _start,@function
_start:
  la    sp, __stacktop
  call  main
  j     0
