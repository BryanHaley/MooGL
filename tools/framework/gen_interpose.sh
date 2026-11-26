#!/bin/sh
# usage: gen_interpose.sh opengl_exports.txt > interpose.s
#
# One interposer for each name OpenGL.framework exports. Each jumps through
# mgl_fw_table, which starts out pointing at Apple's function and is set to
# MooGL's when MooGL.framework loads (framework.c).

awk '
# private names with no published signature: nothing outside Apple calls them
/^(glc|GLC|cgl|gll_)|Dispatch/ { next }
{ name[n++] = $1 }
END {
    print ".section __DATA,__data"
    print ".p2align 3"
    print ".private_extern _mgl_fw_table"
    print "_mgl_fw_table:"
    for (i = 0; i < n; i++) print "    .quad _" name[i]

    print ".private_extern _mgl_fw_count"
    print ".p2align 2"
    print "_mgl_fw_count: .long " n

    print ".section __TEXT,__cstring,cstring_literals"
    for (i = 0; i < n; i++) print "Lname" i ": .asciz \"" name[i] "\""

    print ".section __DATA,__const"
    print ".p2align 3"
    print ".private_extern _mgl_fw_names"
    print "_mgl_fw_names:"
    for (i = 0; i < n; i++) print "    .quad Lname" i

    print ".text"
    for (i = 0; i < n; i++) {
        print ".p2align 2"
        print "Ltramp" i ":"
        print "    adrp x16, _mgl_fw_table@PAGE"
        print "    add x16, x16, _mgl_fw_table@PAGEOFF"
        print "    ldr x16, [x16, #" i * 8 "]"
        print "    br x16"
    }

    print ".section __DATA,__interpose"
    print ".p2align 3"
    for (i = 0; i < n; i++) print "    .quad Ltramp" i ", _" name[i]
}' "$1"
