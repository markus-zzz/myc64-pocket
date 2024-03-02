#include "chars.h"
#include <stdint.h>

#define NULL ((void *)0)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define OSD_DIM_X 256
#define OSD_DIM_Y 64

#define CONT1_KEY ((volatile uint32_t *)0x20000000)

#define CONT1_KEY ((volatile uint32_t *)0x20000000)
#define CONT2_KEY ((volatile uint32_t *)0x20000004)
#define CONT3_KEY ((volatile uint32_t *)0x20000008)
#define CONT4_KEY ((volatile uint32_t *)0x2000000c)
#define CONT1_JOY ((volatile uint32_t *)0x20000010)
#define CONT2_JOY ((volatile uint32_t *)0x20000014)
#define CONT3_JOY ((volatile uint32_t *)0x20000018)
#define CONT4_JOY ((volatile uint32_t *)0x2000001c)
#define CONT1_TRIG ((volatile uint32_t *)0x20000020)
#define CONT2_TRIG ((volatile uint32_t *)0x20000024)
#define CONT3_TRIG ((volatile uint32_t *)0x20000028)
#define CONT4_TRIG ((volatile uint32_t *)0x2000002c)

#define OSD_CTRL ((volatile uint32_t *)0x30000000)
#define KEYB_MASK_0 ((volatile uint32_t *)0x30000004)
#define KEYB_MASK_1 ((volatile uint32_t *)0x30000008)
#define C64_CTRL ((volatile uint32_t *)0x3000000c)

#define TARGET_0 ((volatile uint32_t *)0x40000000)
#define TARGET_4 ((volatile uint32_t *)0x40000004)
#define TARGET_8 ((volatile uint32_t *)0x40000008)
#define TARGET_20 ((volatile uint32_t *)0x40000020)
#define TARGET_24 ((volatile uint32_t *)0x40000024)
#define TARGET_28 ((volatile uint32_t *)0x40000028)
#define TARGET_2C ((volatile uint32_t *)0x4000002c)
#define TARGET_40 ((volatile uint32_t *)0x40000040)
#define TARGET_44 ((volatile uint32_t *)0x40000044)
#define TARGET_48 ((volatile uint32_t *)0x40000048)
#define TARGET_4C ((volatile uint32_t *)0x4000004c)

#define BRIDGE_DPRAM ((volatile uint8_t *)0x70000000)
#define BRIDGE_DS_TABLE ((volatile uint32_t *)0x90000000)

#define KEYB_BIT_dpad_up 0
#define KEYB_BIT_dpad_down 1
#define KEYB_BIT_dpad_left 2
#define KEYB_BIT_dpad_right 3
#define KEYB_BIT_face_a 4
#define KEYB_BIT_face_b 5
#define KEYB_BIT_face_x 6
#define KEYB_BIT_face_y 7
#define KEYB_BIT_trig_l1 8
#define KEYB_BIT_trig_r1 9
#define KEYB_BIT_trig_l2 10
#define KEYB_BIT_trig_r2 11
#define KEYB_BIT_trig_l3 12
#define KEYB_BIT_trig_r3 13
#define KEYB_BIT_face_select 14
#define KEYB_BIT_face_start 15

#define KEYB_POSEDGE(bit)                                                      \
  ((~keyb_p & (1 << (KEYB_BIT_##bit))) && (keyb & (1 << (KEYB_BIT_##bit))))

#define KEYB_DOWN(bit) (keyb & (1 << (KEYB_BIT_##bit)))

struct entry {
  const char *str;
  uint8_t ports;
};

int sel_row = 0;
int sel_col = 0;

const struct entry row_0[] = {
    {"\x1f", 0x71}, {"1", 0x70}, {"2", 0x73}, {"3", 0x10},    {"4", 0x13},
    {"5", 0x20},    {"6", 0x23}, {"7", 0x30}, {"8", 0x33},    {"9", 0x40},
    {"0", 0x43},    {"+", 0x50}, {"-", 0x53}, {"\x1c", 0x60}, {"HOME", 0x63},
    {"DEL", 0x00},  {NULL}};
const struct entry row_1[] = {
    {"CTRL", 0x72}, {"Q", 0x76},    {"W", 0x11},   {"E", 0x16},
    {"R", 0x21},    {"T", 0x26},    {"Y", 0x31},   {"U", 0x36},
    {"I", 0x41},    {"O", 0x46},    {"P", 0x51},   {"@", 0x56},
    {"*", 0x61},    {"\x1e", 0x66}, {"RES", 0xff}, {NULL}};
const struct entry row_2[] = {
    {"STOP", 0x77}, {"A", 0x12}, {"S", 0x15}, {"D", 0x22},   {"F", 0x25},
    {"G", 0x32},    {"H", 0x35}, {"J", 0x42}, {"K", 0x45},   {"L", 0x52},
    {";", 0x62},    {":", 0x55}, {"=", 0x65}, {"RET", 0x01}, {NULL}};
const struct entry row_3[] = {
    {"C=", 0x75},   {"SHFT", 0x17}, {"Z", 0x14},  {"X", 0x27},
    {"C", 0x24},    {"V", 0x37},    {"B", 0x34},  {"N", 0x47},
    {"M", 0x44},    {",", 0x57},    {".", 0x54},  {"/", 0x67},
    {"SHFT", 0x64}, {"DN", 0x07},   {"RT", 0x02}, {NULL}};
const struct entry row_4[] = {{"SPACE", 0x74}, {NULL}};

const struct entry *const rows[] = {row_0, row_1, row_2, row_3, row_4};

// Subset of HID mapping copied from
// https://gist.github.com/MightyPork/6da26e382a7ad91b5496ee55fdc73db2
// Thank you very much MightyPork!

const uint8_t hid2c64[] = {
    0xff, // 0x00 // No key pressed
    0xff, // 0x01 // Keyboard Error Roll Over
    0xff, // 0x02 // Keyboard POST Fail
    0xff, // 0x03 // Keyboard Error Undefined
    0x12, // 0x04 // Keyboard a and A
    0x34, // 0x05 // Keyboard b and B
    0x24, // 0x06 // Keyboard c and C
    0x22, // 0x07 // Keyboard d and D
    0x16, // 0x08 // Keyboard e and E
    0x25, // 0x09 // Keyboard f and F
    0x32, // 0x0a // Keyboard g and G
    0x35, // 0x0b // Keyboard h and H
    0x41, // 0x0c // Keyboard i and I
    0x42, // 0x0d // Keyboard j and J
    0x45, // 0x0e // Keyboard k and K
    0x52, // 0x0f // Keyboard l and L
    0x44, // 0x10 // Keyboard m and M
    0x47, // 0x11 // Keyboard n and N
    0x46, // 0x12 // Keyboard o and O
    0x51, // 0x13 // Keyboard p and P
    0x76, // 0x14 // Keyboard q and Q
    0x21, // 0x15 // Keyboard r and R
    0x15, // 0x16 // Keyboard s and S
    0x26, // 0x17 // Keyboard t and T
    0x36, // 0x18 // Keyboard u and U
    0x37, // 0x19 // Keyboard v and V
    0x11, // 0x1a // Keyboard w and W
    0x27, // 0x1b // Keyboard x and X
    0x31, // 0x1c // Keyboard y and Y
    0x14, // 0x1d // Keyboard z and Z

    0x70, // 0x1e // Keyboard 1 and !
    0x73, // 0x1f // Keyboard 2 and @
    0x10, // 0x20 // Keyboard 3 and #
    0x13, // 0x21 // Keyboard 4 and $
    0x20, // 0x22 // Keyboard 5 and %
    0x23, // 0x23 // Keyboard 6 and ^
    0x30, // 0x24 // Keyboard 7 and &
    0x33, // 0x25 // Keyboard 8 and *
    0x40, // 0x26 // Keyboard 9 and (
    0x43, // 0x27 // Keyboard 0 and )

    0x01, // 0x28 // Keyboard Return (ENTER)
    0x77, // 0x29 // Keyboard ESCAPE
    0x00, // 0x2a // Keyboard DELETE (Backspace)
    0x72, // 0x2b // Keyboard Tab
    0x74, // 0x2c // Keyboard Spacebar
    0x50, // 0x2d // Keyboard - and _
    0x53, // 0x2e // Keyboard = and +
    0x56, // 0x2f // Keyboard [ and {
    0x61, // 0x30 // Keyboard ] and }
    0x65, // 0x31 // Keyboard \ and |
    0x65, // 0x32 // Keyboard Non-US # and ~
    0x62, // 0x33 // Keyboard ; and :
    0x55, // 0x34 // Keyboard ' and "
    0x71, // 0x35 // Keyboard ` and ~
    0x57, // 0x36 // Keyboard , and <
    0x54, // 0x37 // Keyboard . and >
    0x67, // 0x38 // Keyboard / and ?
    0x77, // 0x39 // Keyboard Caps Lock

    0x04, // 0x3a // Keyboard F1
    0xff, // 0x3b // Keyboard F2
    0x05, // 0x3c // Keyboard F3
    0xff, // 0x3d // Keyboard F4
    0x06, // 0x3e // Keyboard F5
    0xff, // 0x3f // Keyboard F6
    0x03, // 0x40 // Keyboard F7
    0xff, // 0x41 // Keyboard F8
    0xff, // 0x42 // Keyboard F9
    0xff, // 0x43 // Keyboard F10
    0xff, // 0x44 // Keyboard F11
    0xff, // 0x45 // Keyboard F12

    0xff, // 0x46 // Keyboard Print Screen
    0xff, // 0x47 // Keyboard Scroll Lock
    0xff, // 0x48 // Keyboard Pause
    0x60, // 0x49 // Keyboard Insert
    0x63, // 0x4a // Keyboard Home
    0xff, // 0x4b // Keyboard Page Up
    0x66, // 0x4c // Keyboard Delete Forward
    0xff, // 0x4d // Keyboard End
    0xff, // 0x4e // Keyboard Page Down
    0x02, // 0x4f // Keyboard Right Arrow
    0xff, // 0x50 // Keyboard Left Arrow
    0x07, // 0x51 // Keyboard Down Arrow
    0xff, // 0x52 // Keyboard Up Arrow
};

unsigned row_length(unsigned row_idx) {
  const struct entry *p = rows[row_idx];
  unsigned len = 0;
  while (p->str) {
    len++;
    p++;
  }
  return len;
}

static void set_pixel(unsigned x, unsigned y) {
  if (x >= OSD_DIM_X || y >= OSD_DIM_Y)
    return;
  volatile unsigned char *p = (volatile unsigned char *)0x10000000;
  unsigned idx = y * OSD_DIM_X / 8 + x / 8;
  unsigned bit = 7 - x % 8;
  p[idx] |= 1 << bit;
}

static void clr_pixel(unsigned x, unsigned y) {
  if (x >= OSD_DIM_X || y >= OSD_DIM_Y)
    return;
  volatile unsigned char *p = (volatile unsigned char *)0x10000000;
  unsigned idx = y * OSD_DIM_X / 8 + x / 8;
  unsigned bit = 7 - x % 8;
  p[idx] &= ~(1 << bit);
}

static void draw_char_bitmap(int x, int y, const unsigned char *p,
                             int selected) {
  for (int i = 0; i < 8; i++) {
    char q = p[i];
    for (int j = 0; j < 8; j++) {
      if (((q >> (7 - j)) & 1) != selected)
        set_pixel(x + j, y + i);
      else
        clr_pixel(x + j, y + i);
    }
  }
}

static char to_upper(char c) {
  if ('a' <= c && c <= 'z')
    return c - ('a' - 'A');
  else
    return c;
}

void put_char(int x, int y, char c, int invert) {
  c = to_upper(c);
  const uint8_t *p = &chars_bin[(c & 0x3f) * 8];
  draw_char_bitmap(x, y, p, invert ? 1 : 0);
}

unsigned put_str(int x, int y, const char *str, int invert) {
  while (*str != '\0') {
    put_char(x, y, *str, invert);
    x += 8;
    str++;
  }
  return x;
}

unsigned put_hex8(int x, int y, uint8_t val, int invert) {
  const char *digits = "0123456789ABCDEF";
  for (unsigned i = 0; i < 2; i++) {
    put_char(x, y, digits[(val >> ((1 - i) * 4) & 0xf)], invert);
    x += 8;
  }
  return x;
}

unsigned put_hex16(int x, int y, uint16_t val, int invert) {
  const char *digits = "0123456789ABCDEF";
  for (unsigned i = 0; i < 4; i++) {
    put_char(x, y, digits[(val >> ((3 - i) * 4) & 0xf)], invert);
    x += 8;
  }
  return x;
}

uint32_t get_ds_length(uint16_t slot_id) {
  volatile uint32_t *p = BRIDGE_DS_TABLE;

  for (unsigned idx = 0; idx < 32; idx++) {
    uint16_t ds_slot_id = p[idx * 2 + 0] & 0xffff;
    uint32_t ds_length = p[idx * 2 + 1];
    if (ds_slot_id == slot_id)
      return ds_length;
  }
  return 0;
}

void load_prg(uint16_t slot_id) {
  volatile uint8_t *p = (volatile uint8_t *)0x70000000;
  volatile uint8_t *RAM = (volatile uint8_t *)0x50000000;
  uint16_t slot_length = get_ds_length(slot_id);
  if (!slot_length)
    return;

  // First load the 16 bit header with PrgStartAddr
  *TARGET_20 = slot_id; // slot-id
  *TARGET_24 = 0;       // slot-offset
  *TARGET_28 = 0x70000000;
  *TARGET_2C = 2; // length
  *TARGET_0 = 0x636D0180;
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;
  uint16_t PrgSize = slot_length - 2;
  uint16_t PrgStartAddr = (p[1] << 8) | p[0];

  // Update various zero page pointers to adjust for loaded program.
  // - Pointer to beginning of variable area. (End of program plus 1.)
  // - Pointer to beginning of array variable area.
  // - Pointer to end of array variable area.
  // - Load address read from input file and pointer to current byte during
  // LOAD/VERIFY from serial bus.
  //   End address after LOAD/VERIFY from serial bus or datasette.
  // For details see https://sta.c64.org/cbm64mem.html and
  // VICE source: src/c64/c64mem.c:mem_set_basic_text()
  uint16_t PrgEndAddr = PrgStartAddr + PrgSize;
#define RAM_W16(addr, val)                                                     \
  RAM[addr] = (val)&0xff;                                                      \
  RAM[(addr) + 1] = (val) >> 8;
  RAM_W16(0x2d, PrgEndAddr);
  RAM_W16(0x2f, PrgEndAddr);
  RAM_W16(0x31, PrgEndAddr);
  RAM_W16(0xae, PrgEndAddr);

  volatile uint8_t *q = &RAM[PrgStartAddr];

  const uint32_t buf_size = 256;
  uint32_t slot_offset = 2;
  uint32_t idx = 0;

  while (slot_offset < slot_length) {
    uint32_t chunk_size = MIN(slot_length - slot_offset, buf_size);
    *TARGET_20 = slot_id;
    *TARGET_24 = slot_offset;
    *TARGET_28 = 0x70000000;
    *TARGET_2C = chunk_size;
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;

    // Write into C64 RAM
    for (uint32_t i = 0; i < chunk_size; i++) {
      if (idx < PrgSize)
        q[idx++] = p[i];
    }

    slot_offset += chunk_size;
  }
}

void load_rom(uint16_t slot_id, volatile uint8_t *dst, uint32_t slot_length) {
  volatile uint8_t *p = (volatile uint8_t *)0x70000000;
  // uint16_t slot_length = get_ds_length(slot_id);
  if (!slot_length)
    return;

  const uint32_t buf_size = 256;
  uint32_t slot_offset = 0;

  while (slot_offset < slot_length) {
    uint32_t chunk_size = MIN(slot_length - slot_offset, buf_size);
    *TARGET_20 = slot_id;
    *TARGET_24 = slot_offset;
    *TARGET_28 = 0x70000000;
    *TARGET_2C = chunk_size;
    *TARGET_0 = 0x636D0180;
    while ((*TARGET_0 >> 16) != 0x6F6B)
      ;

    // Write into C64 ROM
    for (uint32_t i = 0; i < chunk_size; i++) {
      *dst++ = p[i];
    }

    slot_offset += chunk_size;
  }
}

int main(void) {

  // Wait for previous command to finish
  while ((*TARGET_0 >> 16) != 0x6F6B)
    ;

  // Load BASIC ROM
  load_rom(200, (volatile uint8_t *)0x50010000, 8192);
  // Load CHAR ROM
  load_rom(201, (volatile uint8_t *)0x50020000, 4096);
  // Load KERNAL ROM
  load_rom(202, (volatile uint8_t *)0x50030000, 8192);

  *C64_CTRL = 1; // Release reset for MyC64

  uint32_t keyb_p = 0;
  uint32_t keyb = 0;

  int osd_keyb_on = 0;

  sel_row = 0;
  sel_col = 0;

  while (1) {
#define KEYB_MASK_KEY(x)                                                       \
  keyb_mask |= 1ULL << ((((x) >> 4) & 0xf) * 8 + ((x)&0xf))
    uint64_t keyb_mask = 0;
    keyb = *CONT1_KEY;

    if (KEYB_POSEDGE(face_select)) {
      osd_keyb_on = !osd_keyb_on;
      *OSD_CTRL = osd_keyb_on;
    }

    if (osd_keyb_on) {
      if (KEYB_POSEDGE(dpad_up)) {
        if (sel_row > 0)
          sel_row--;
        sel_col = MIN(sel_col, row_length(sel_row) - 1);
      } else if (KEYB_POSEDGE(dpad_down)) {
        if (sel_row < sizeof(rows) / sizeof(rows[0]) - 1)
          sel_row++;
        sel_col = MIN(sel_col, row_length(sel_row) - 1);
      } else if (KEYB_POSEDGE(dpad_left)) {
        if (sel_col > 0)
          sel_col--;
      } else if (KEYB_POSEDGE(dpad_right)) {
        sel_col++;
        sel_col = MIN(sel_col, row_length(sel_row) - 1);
      }

      if (KEYB_DOWN(face_a)) {
        const struct entry *row = rows[sel_row];
        const struct entry *e = &row[sel_col];
        KEYB_MASK_KEY(e->ports);
      }

      if (KEYB_DOWN(trig_l1)) {
        // Apply LSHIFT
        KEYB_MASK_KEY(0x17);
      }
    }

    if (KEYB_POSEDGE(face_start)) {
      load_prg(100);
    }

    if (1) { // (*CONT3_KEY >> 28) == 0x4) { // Docked keyboard
      uint8_t hid_mod = *CONT3_KEY >> 8;
      uint8_t hid_keys[6];
      hid_keys[0] = *CONT3_JOY >> 24;
      hid_keys[1] = *CONT3_JOY >> 16;
      hid_keys[2] = *CONT3_JOY >> 8;
      hid_keys[3] = *CONT3_JOY >> 0;
      hid_keys[4] = *CONT3_TRIG >> 8;
      hid_keys[5] = *CONT3_TRIG >> 0;
      for (unsigned i = 0; i < 6; i++) {
        if (hid_keys[i] < sizeof(hid2c64)) {
          uint8_t ports = hid2c64[hid_keys[i]];
          if (ports == 0xff)
            continue;
          KEYB_MASK_KEY(ports);
        }
      }

      // https://wiki.osdev.org/USB_Human_Interface_Devices
      if (hid_mod & (1 << 0)) {
        // The Commodore key
        KEYB_MASK_KEY(0x75);
      }
      if (hid_mod & (1 << 1)) {
        // Left shift
        KEYB_MASK_KEY(0x17);
      }
      if (hid_mod & (1 << 5)) {
        // Right shift
        KEYB_MASK_KEY(0x64);
      }
    }

    if (osd_keyb_on) {
      // Draw keyboard to OSD buffer
      for (int i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        const struct entry *row = rows[i];
        int offset = 0;
        for (int j = 0; row[j].str != NULL; j++) {
          const char *str = row[j].str;
          for (int k = 0; str[k] != '\0'; k++) {
            put_char(offset, 2 + i * 10, str[k], sel_row == i && sel_col == j);
            offset += 8;
          }
          offset += 2;
        }
      }
    }

    *KEYB_MASK_0 = keyb_mask;
    *KEYB_MASK_1 = keyb_mask >> 32;

    keyb_p = keyb;
  }

  return 0;
}
