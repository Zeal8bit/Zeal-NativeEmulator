#include <stdio.h>
#include "debugger/debugger_types.h"

const instr_data_t disassembled_z80_opcodes[256] = {
    [0x0] = {
        .fmt = "nop",
        .size = 1,
    },
    [0x1] = {
        .fmt = "ld     bc, 0x%04x",
        .size = 3,
    },
    [0x2] = {
        .fmt = "ld     (bc), a",
        .size = 1,
    },
    [0x3] = {
        .fmt = "inc    bc",
        .size = 1,
    },
    [0x4] = {
        .fmt = "inc    b",
        .size = 1,
    },
    [0x5] = {
        .fmt = "dec    b",
        .size = 1,
    },
    [0x6] = {
        .fmt = "ld     b, 0x%x",
        .size = 2,
    },
    [0x7] = {
        .fmt = "rlca",
        .size = 1,
    },
    [0x8] = {
        .fmt = "ex     af, af'",
        .size = 1,
    },
    [0x9] = {
        .fmt = "add    hl, bc",
        .size = 1,
    },
    [0xa] = {
        .fmt = "ld     a, (bc)",
        .size = 1,
    },
    [0xb] = {
        .fmt = "dec    bc",
        .size = 1,
    },
    [0xc] = {
        .fmt = "inc    c",
        .size = 1,
    },
    [0xd] = {
        .fmt = "dec    c",
        .size = 1,
    },
    [0xe] = {
        .fmt = "ld     c, 0x%x",
        .size = 2,
    },
    [0xf] = {
        .fmt = "rrca",
        .size = 1,
    },
    [0x10] = {
        .fmt     = "djnz   0x%04x",
        .fmt_lab = "djnz   %s",
        .size  = 2,
        .label = 1,
    },
    [0x11] = {
        .fmt = "ld     de, 0x%04x",
        .size = 3,
    },
    [0x12] = {
        .fmt = "ld     (de), a",
        .size = 1,
    },
    [0x13] = {
        .fmt = "inc    de",
        .size = 1,
    },
    [0x14] = {
        .fmt = "inc    d",
        .size = 1,
    },
    [0x15] = {
        .fmt = "dec    d",
        .size = 1,
    },
    [0x16] = {
        .fmt = "ld     d, 0x%x",
        .size = 2,
    },
    [0x17] = {
        .fmt = "rla",
        .size = 1,
    },
    [0x18] = {
        .fmt     = "jr     0x%04x",
        .fmt_lab = "jr     %s",
        .size  = 2,
        .label = 1,
    },
    [0x19] = {
        .fmt = "add    hl, de",
        .size = 1,
    },
    [0x1a] = {
        .fmt = "ld     a,(de)",
        .size = 1,
    },
    [0x1b] = {
        .fmt = "dec    de",
        .size = 1,
    },
    [0x1c] = {
        .fmt = "inc    e",
        .size = 1,
    },
    [0x1d] = {
        .fmt = "dec    e",
        .size = 1,
    },
    [0x1e] = {
        .fmt = "ld     e, 0x%x",
        .size = 2,
    },
    [0x1f] = {
        .fmt = "rra",
        .size = 1,
    },
    [0x20] = {
        .fmt     = "jr     nz, 0x%04x",
        .fmt_lab = "jr     nz, %s",
        .size  = 2,
        .label = 1,
    },
    [0x21] = {
        .fmt = "ld     hl, 0x%04x",
        .size = 3,
    },
    [0x22] = {
        .fmt     = "ld     (0x%04x), hl",
        .fmt_lab = "ld     (%s), hl",
        .size  = 3,
        .label = 1,
    },
    [0x23] = {
        .fmt = "inc    hl",
        .size = 1,
    },
    [0x24] = {
        .fmt = "inc    h",
        .size = 1,
    },
    [0x25] = {
        .fmt = "dec    h",
        .size = 1,
    },
    [0x26] = {
        .fmt = "ld     h, 0x%x",
        .size = 2,
    },
    [0x27] = {
        .fmt = "daa",
        .size = 1,
    },
    [0x28] = {
        .fmt     = "jr     z, 0x%04x",
        .fmt_lab = "jr     z, %s",
        .size  = 2,
        .label = 1,
    },
    [0x29] = {
        .fmt = "add    hl, hl",
        .size = 1,
    },
    [0x2a] = {
        .fmt     = "ld     hl, (0x%04x)",
        .fmt_lab = "ld     hl, (%s)",
        .size  = 3,
        .label = 1,
    },
    [0x2b] = {
        .fmt = "dec    hl",
        .size = 1,
    },
    [0x2c] = {
        .fmt = "inc    l",
        .size = 1,
    },
    [0x2d] = {
        .fmt = "dec    l",
        .size = 1,
    },
    [0x2e] = {
        .fmt = "ld     l, 0x%x",
        .size = 2,
    },
    [0x2f] = {
        .fmt = "cpl",
        .size = 1,
    },
    [0x30] = {
        .fmt     = "jr     nc, 0x%04x",
        .fmt_lab = "jr     nc, %s",
        .size  = 2,
        .label = 1,
    },
    [0x31] = {
        .fmt = "ld     sp, 0x%04x",
        .size = 3,
    },
    [0x32] = {
        .fmt     = "ld     (0x%04x), a",
        .fmt_lab = "ld     (%s), a",
        .size  = 3,
        .label = 1,
    },
    [0x33] = {
        .fmt = "inc    sp",
        .size = 1,
    },
    [0x34] = {
        .fmt = "inc    (hl)",
        .size = 1,
    },
    [0x35] = {
        .fmt = "dec    (hl)",
        .size = 1,
    },
    [0x36] = {
        .fmt = "ld     (hl), 0x%x",
        .size = 2,
    },
    [0x37] = {
        .fmt = "scf",
        .size = 1,
    },
    [0x38] = {
        .fmt     = "jr     c, 0x%04x",
        .fmt_lab = "jr     c, %s",
        .size  = 2,
        .label = 1,
    },
    [0x39] = {
        .fmt = "add    hl, sp",
        .size = 1,
    },
    [0x3a] = {
        .fmt     = "ld     a, (0x%04x)",
        .fmt_lab = "ld     a, (%s)",
        .size  = 3,
        .label = 1,
    },
    [0x3b] = {
        .fmt = "dec    sp",
        .size = 1,
    },
    [0x3c] = {
        .fmt = "inc    a",
        .size = 1,
    },
    [0x3d] = {
        .fmt = "dec    a",
        .size = 1,
    },
    [0x3e] = {
        .fmt = "ld     a, 0x%x",
        .size = 2,
    },
    [0x3f] = {
        .fmt = "ccf",
        .size = 1,
    },
    [0x40] = {
        .fmt = "ld     b, b",
        .size = 1,
    },
    [0x41] = {
        .fmt = "ld     b, c",
        .size = 1,
    },
    [0x42] = {
        .fmt = "ld     b, d",
        .size = 1,
    },
    [0x43] = {
        .fmt = "ld     b, e",
        .size = 1,
    },
    [0x44] = {
        .fmt = "ld     b, h",
        .size = 1,
    },
    [0x45] = {
        .fmt = "ld     b, l",
        .size = 1,
    },
    [0x46] = {
        .fmt = "ld     b, (hl)",
        .size = 1,
    },
    [0x47] = {
        .fmt = "ld     b, a",
        .size = 1,
    },
    [0x48] = {
        .fmt = "ld     c, b",
        .size = 1,
    },
    [0x49] = {
        .fmt = "ld     c, c",
        .size = 1,
    },
    [0x4a] = {
        .fmt = "ld     c, d",
        .size = 1,
    },
    [0x4b] = {
        .fmt = "ld     c, e",
        .size = 1,
    },
    [0x4c] = {
        .fmt = "ld     c, h",
        .size = 1,
    },
    [0x4d] = {
        .fmt = "ld     c, l",
        .size = 1,
    },
    [0x4e] = {
        .fmt = "ld     c, (hl)",
        .size = 1,
    },
    [0x4f] = {
        .fmt = "ld     c, a",
        .size = 1,
    },
    [0x50] = {
        .fmt = "ld     d, b",
        .size = 1,
    },
    [0x51] = {
        .fmt = "ld     d, c",
        .size = 1,
    },
    [0x52] = {
        .fmt = "ld     d, d",
        .size = 1,
    },
    [0x53] = {
        .fmt = "ld     d, e",
        .size = 1,
    },
    [0x54] = {
        .fmt = "ld     d, h",
        .size = 1,
    },
    [0x55] = {
        .fmt = "ld     d, l",
        .size = 1,
    },
    [0x56] = {
        .fmt = "ld     d, (hl)",
        .size = 1,
    },
    [0x57] = {
        .fmt = "ld     d, a",
        .size = 1,
    },
    [0x58] = {
        .fmt = "ld     e, b",
        .size = 1,
    },
    [0x59] = {
        .fmt = "ld     e, c",
        .size = 1,
    },
    [0x5a] = {
        .fmt = "ld     e, d",
        .size = 1,
    },
    [0x5b] = {
        .fmt = "ld     e, e",
        .size = 1,
    },
    [0x5c] = {
        .fmt = "ld     e, h",
        .size = 1,
    },
    [0x5d] = {
        .fmt = "ld     e, l",
        .size = 1,
    },
    [0x5e] = {
        .fmt = "ld     e, (hl)",
        .size = 1,
    },
    [0x5f] = {
        .fmt = "ld     e, a",
        .size = 1,
    },
    [0x60] = {
        .fmt = "ld     h, b",
        .size = 1,
    },
    [0x61] = {
        .fmt = "ld     h, c",
        .size = 1,
    },
    [0x62] = {
        .fmt = "ld     h, d",
        .size = 1,
    },
    [0x63] = {
        .fmt = "ld     h, e",
        .size = 1,
    },
    [0x64] = {
        .fmt = "ld     h, h",
        .size = 1,
    },
    [0x65] = {
        .fmt = "ld     h, l",
        .size = 1,
    },
    [0x66] = {
        .fmt = "ld     h, (hl)",
        .size = 1,
    },
    [0x67] = {
        .fmt = "ld     h, a",
        .size = 1,
    },
    [0x68] = {
        .fmt = "ld     l, b",
        .size = 1,
    },
    [0x69] = {
        .fmt = "ld     l, c",
        .size = 1,
    },
    [0x6a] = {
        .fmt = "ld     l, d",
        .size = 1,
    },
    [0x6b] = {
        .fmt = "ld     l, e",
        .size = 1,
    },
    [0x6c] = {
        .fmt = "ld     l, h",
        .size = 1,
    },
    [0x6d] = {
        .fmt = "ld     l, l",
        .size = 1,
    },
    [0x6e] = {
        .fmt = "ld     l, (hl)",
        .size = 1,
    },
    [0x6f] = {
        .fmt = "ld     l, a",
        .size = 1,
    },
    [0x70] = {
        .fmt = "ld     (hl), b",
        .size = 1,
    },
    [0x71] = {
        .fmt = "ld     (hl), c",
        .size = 1,
    },
    [0x72] = {
        .fmt = "ld     (hl), d",
        .size = 1,
    },
    [0x73] = {
        .fmt = "ld     (hl), e",
        .size = 1,
    },
    [0x74] = {
        .fmt = "ld     (hl), h",
        .size = 1,
    },
    [0x75] = {
        .fmt = "ld     (hl), l",
        .size = 1,
    },
    [0x76] = {
        .fmt = "halt",
        .size = 1,
    },
    [0x77] = {
        .fmt = "ld     (hl), a",
        .size = 1,
    },
    [0x78] = {
        .fmt = "ld     a, b",
        .size = 1,
    },
    [0x79] = {
        .fmt = "ld     a, c",
        .size = 1,
    },
    [0x7a] = {
        .fmt = "ld     a, d",
        .size = 1,
    },
    [0x7b] = {
        .fmt = "ld     a, e",
        .size = 1,
    },
    [0x7c] = {
        .fmt = "ld     a, h",
        .size = 1,
    },
    [0x7d] = {
        .fmt = "ld     a, l",
        .size = 1,
    },
    [0x7e] = {
        .fmt = "ld     a, (hl)",
        .size = 1,
    },
    [0x7f] = {
        .fmt = "ld     a, a",
        .size = 1,
    },
    [0x80] = {
        .fmt = "add    a, b",
        .size = 1,
    },
    [0x81] = {
        .fmt = "add    a, c",
        .size = 1,
    },
    [0x82] = {
        .fmt = "add    a, d",
        .size = 1,
    },
    [0x83] = {
        .fmt = "add    a, e",
        .size = 1,
    },
    [0x84] = {
        .fmt = "add    a, h",
        .size = 1,
    },
    [0x85] = {
        .fmt = "add    a, l",
        .size = 1,
    },
    [0x86] = {
        .fmt = "add    a, (hl)",
        .size = 1,
    },
    [0x87] = {
        .fmt = "add    a, a",
        .size = 1,
    },
    [0x88] = {
        .fmt = "adc    a, b",
        .size = 1,
    },
    [0x89] = {
        .fmt = "adc    a, c",
        .size = 1,
    },
    [0x8a] = {
        .fmt = "adc    a, d",
        .size = 1,
    },
    [0x8b] = {
        .fmt = "adc    a, e",
        .size = 1,
    },
    [0x8c] = {
        .fmt = "adc    a, h",
        .size = 1,
    },
    [0x8d] = {
        .fmt = "adc    a, l",
        .size = 1,
    },
    [0x8e] = {
        .fmt = "adc    a, (hl)",
        .size = 1,
    },
    [0x8f] = {
        .fmt = "adc    a, a",
        .size = 1,
    },
    [0x90] = {
        .fmt = "sub    b",
        .size = 1,
    },
    [0x91] = {
        .fmt = "sub    c",
        .size = 1,
    },
    [0x92] = {
        .fmt = "sub    d",
        .size = 1,
    },
    [0x93] = {
        .fmt = "sub    e",
        .size = 1,
    },
    [0x94] = {
        .fmt = "sub    h",
        .size = 1,
    },
    [0x95] = {
        .fmt = "sub    l",
        .size = 1,
    },
    [0x96] = {
        .fmt = "sub    (hl)",
        .size = 1,
    },
    [0x97] = {
        .fmt = "sub    a",
        .size = 1,
    },
    [0x98] = {
        .fmt = "sbc    a, b",
        .size = 1,
    },
    [0x99] = {
        .fmt = "sbc    a, c",
        .size = 1,
    },
    [0x9a] = {
        .fmt = "sbc    a, d",
        .size = 1,
    },
    [0x9b] = {
        .fmt = "sbc    a, e",
        .size = 1,
    },
    [0x9c] = {
        .fmt = "sbc    a, h",
        .size = 1,
    },
    [0x9d] = {
        .fmt = "sbc    a, l",
        .size = 1,
    },
    [0x9e] = {
        .fmt = "sbc    a, (hl)",
        .size = 1,
    },
    [0x9f] = {
        .fmt = "sbc    a, a",
        .size = 1,
    },
    [0xa0] = {
        .fmt = "and    b",
        .size = 1,
    },
    [0xa1] = {
        .fmt = "and    c",
        .size = 1,
    },
    [0xa2] = {
        .fmt = "and    d",
        .size = 1,
    },
    [0xa3] = {
        .fmt = "and    e",
        .size = 1,
    },
    [0xa4] = {
        .fmt = "and    h",
        .size = 1,
    },
    [0xa5] = {
        .fmt = "and    l",
        .size = 1,
    },
    [0xa6] = {
        .fmt = "and    (hl)",
        .size = 1,
    },
    [0xa7] = {
        .fmt = "and    a",
        .size = 1,
    },
    [0xa8] = {
        .fmt = "xor    b",
        .size = 1,
    },
    [0xa9] = {
        .fmt = "xor    c",
        .size = 1,
    },
    [0xaa] = {
        .fmt = "xor    d",
        .size = 1,
    },
    [0xab] = {
        .fmt = "xor    e",
        .size = 1,
    },
    [0xac] = {
        .fmt = "xor    h",
        .size = 1,
    },
    [0xad] = {
        .fmt = "xor    l",
        .size = 1,
    },
    [0xae] = {
        .fmt = "xor    (hl)",
        .size = 1,
    },
    [0xaf] = {
        .fmt = "xor    a",
        .size = 1,
    },
    [0xb0] = {
        .fmt = "or     b",
        .size = 1,
    },
    [0xb1] = {
        .fmt = "or     c",
        .size = 1,
    },
    [0xb2] = {
        .fmt = "or     d",
        .size = 1,
    },
    [0xb3] = {
        .fmt = "or     e",
        .size = 1,
    },
    [0xb4] = {
        .fmt = "or     h",
        .size = 1,
    },
    [0xb5] = {
        .fmt = "or     l",
        .size = 1,
    },
    [0xb6] = {
        .fmt = "or     (hl)",
        .size = 1,
    },
    [0xb7] = {
        .fmt = "or     a",
        .size = 1,
    },
    [0xb8] = {
        .fmt = "cp     b",
        .size = 1,
    },
    [0xb9] = {
        .fmt = "cp     c",
        .size = 1,
    },
    [0xba] = {
        .fmt = "cp     d",
        .size = 1,
    },
    [0xbb] = {
        .fmt = "cp     e",
        .size = 1,
    },
    [0xbc] = {
        .fmt = "cp     h",
        .size = 1,
    },
    [0xbd] = {
        .fmt = "cp     l",
        .size = 1,
    },
    [0xbe] = {
        .fmt = "cp     (hl)",
        .size = 1,
    },
    [0xbf] = {
        .fmt = "cp     a",
        .size = 1,
    },
    [0xc0] = {
        .fmt = "ret    nz",
        .size = 1,
    },
    [0xc1] = {
        .fmt = "pop    bc",
        .size = 1,
    },
    [0xc2] = {
        .fmt     = "jp     nz, 0x%04x",
        .fmt_lab = "jp     nz, %s",
        .size  = 3,
        .label = 1,
    },
    [0xc3] = {
        .fmt     = "jp     0x%04x",
        .fmt_lab = "jp     %s",
        .size  = 3,
        .label = 1,
    },
    [0xc4] = {
        .fmt     = "call   nz, 0x%04x",
        .fmt_lab = "call   nz, %s",
        .size  = 3,
        .label = 1,
    },
    [0xc5] = {
        .fmt = "push   bc",
        .size = 1,
    },
    [0xc6] = {
        .fmt = "add    a, 0x%x",
        .size = 2,
    },
    [0xc7] = {
        .fmt = "rst    00",
        .size = 1,
    },
    [0xc8] = {
        .fmt = "ret    z",
        .size = 1,
    },
    [0xc9] = {
        .fmt = "ret",
        .size = 1,
    },
    [0xca] = {
        .fmt     = "jp     z, 0x%04x",
        .fmt_lab = "jp     z, %s",
        .size  = 3,
        .label = 1,
    },
    [0xcc] = {
        .fmt     = "call   z, 0x%04x",
        .fmt_lab = "call   z, %s",
        .size  = 3,
        .label = 1,
    },
    [0xcd] = {
        .fmt     = "call   0x%04x",
        .fmt_lab = "call   %s",
        .size  = 3,
        .label = 1,
    },
    [0xce] = {
        .fmt = "adc    a, 0x%x",
        .size = 2,
    },
    [0xcf] = {
        .fmt = "rst    08",
        .size = 1,
    },
    [0xd0] = {
        .fmt = "ret    nc",
        .size = 1,
    },
    [0xd1] = {
        .fmt = "pop    de",
        .size = 1,
    },
    [0xd2] = {
        .fmt     = "jp     nc, 0x%04x",
        .fmt_lab = "jp     nc, %s",
        .size  = 3,
        .label = 1,
    },
    [0xd3] = {
        .fmt = "out    (0x%x), a",
        .size = 2,
    },
    [0xd4] = {
        .fmt     = "call   nc, 0x%04x",
        .fmt_lab = "call   nc, %s",
        .size  = 3,
        .label = 1,
    },
    [0xd5] = {
        .fmt = "push   de",
        .size = 1,
    },
    [0xd6] = {
        .fmt = "sub    0x%x",
        .size = 2,
    },
    [0xd7] = {
        .fmt = "rst    10",
        .size = 1,
    },
    [0xd8] = {
        .fmt = "ret    c",
        .size = 1,
    },
    [0xd9] = {
        .fmt = "exx",
        .size = 1,
    },
    [0xda] = {
        .fmt     = "jp     c, 0x%04x",
        .fmt_lab = "jp     c, %s",
        .size  = 3,
        .label = 1,
    },
    [0xdb] = {
        .fmt = "in     a, (0x%x)",
        .size = 2,
    },
    [0xdc] = {
        .fmt     = "call   c, 0x%04x",
        .fmt_lab = "call   c, %s",
        .size  = 3,
        .label = 1,
    },
    [0xde] = {
        .fmt = "sbc    a, 0x%x",
        .size = 2,
    },
    [0xdf] = {
        .fmt = "rst    18",
        .size = 1,
    },
    [0xe0] = {
        .fmt = "ret    po",
        .size = 1,
    },
    [0xe1] = {
        .fmt = "pop    hl",
        .size = 1,
    },
    [0xe2] = {
        .fmt     = "jp     po, 0x%04x",
        .fmt_lab = "jp     po, %s",
        .size  = 3,
        .label = 1,
    },
    [0xe3] = {
        .fmt = "ex     (sp), hl",
        .size = 1,
    },
    [0xe4] = {
        .fmt     = "call   po, 0x%04x",
        .fmt_lab = "call   po, %s",
        .size  = 3,
        .label = 1,
    },
    [0xe5] = {
        .fmt = "push   hl",
        .size = 1,
    },
    [0xe6] = {
        .fmt = "and    0x%x",
        .size = 2,
    },
    [0xe7] = {
        .fmt = "rst    20",
        .size = 1,
    },
    [0xe8] = {
        .fmt = "ret    pe",
        .size = 1,
    },
    [0xe9] = {
        .fmt = "jp     (hl)",
        .size = 1,
    },
    [0xea] = {
        .fmt     = "jp     pe, 0x%04x",
        .fmt_lab = "jp     pe, %s",
        .size  = 3,
        .label = 1,
    },
    [0xeb] = {
        .fmt = "ex     de, hl",
        .size = 1,
    },
    [0xec] = {
        .fmt     = "call   pe, 0x%04x",
        .fmt_lab = "call   pe, %s",
        .size  = 3,
        .label = 1,
    },
    [0xee] = {
        .fmt = "xor    0x%x",
        .size = 2,
    },
    [0xef] = {
        .fmt = "rst    28",
        .size = 1,
    },
    [0xf0] = {
        .fmt = "ret    p",
        .size = 1,
    },
    [0xf1] = {
        .fmt = "pop    af",
        .size = 1,
    },
    [0xf2] = {
        .fmt     = "jp     p, 0x%04x",
        .fmt_lab = "jp     p, %s",
        .size  = 3,
        .label = 1,
    },
    [0xf3] = {
        .fmt = "di",
        .size = 1,
    },
    [0xf4] = {
        .fmt     = "call   p, 0x%04x",
        .fmt_lab = "call   p, %s",
        .size  = 3,
        .label = 1,
    },
    [0xf5] = {
        .fmt = "push   af",
        .size = 1,
    },
    [0xf6] = {
        .fmt = "or     0x%x",
        .size = 2,
    },
    [0xf7] = {
        .fmt = "rst    30",
        .size = 1,
    },
    [0xf8] = {
        .fmt = "ret    m",
        .size = 1,
    },
    [0xf9] = {
        .fmt = "ld     sp, hl",
        .size = 1,
    },
    [0xfa] = {
        .fmt     = "jp     m, 0x%04x",
        .fmt_lab = "jp     m, %s",
        .size  = 3,
        .label = 1,
    },
    [0xfb] = {
        .fmt = "ei",
        .size = 1,
    },
    [0xfc] = {
        .fmt     = "call   m, 0x%04x",
        .fmt_lab = "call   m, %s",
        .size  = 3,
        .label = 1,
    },
    [0xfe] = {
        .fmt = "cp     0x%x",
        .size = 2,
    },
    [0xff] = {
        .fmt = "rst    38",
        .size = 1,
    },
};
