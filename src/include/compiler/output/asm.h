/*
 Copyright (c) 2012-2013, The Saffire Group
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the Saffire Group the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef __OUTPUT_ASM_H__
#define __OUTPUT_ASM_H__

    #include "compiler/bytecode.h"
    #include "general/smm.h"

    #define ASM_LINE_TYPE_OP_LABEL      1
    #define ASM_LINE_TYPE_OP_STRING     2
    #define ASM_LINE_TYPE_OP_CODE       3
    #define ASM_LINE_TYPE_OP_REALNUM    4
    #define ASM_LINE_TYPE_OP_NUM        5
    #define ASM_LINE_TYPE_OP_COMPARE    6
    #define ASM_LINE_TYPE_OP_ID         7
    #define ASM_LINE_TYPE_OP_OPERATOR   8
    #define ASM_LINE_TYPE_OP_REGEX      9

    #define ASM_LINE_TYPE_LABEL         1
    #define ASM_LINE_TYPE_CODE          2

    typedef struct _asm_opr {
        int type;
        struct {            // Not a union, but a struct. This way we can always figure out easily if "s" or "r" has to be freed.
            char    *s;
            char    *r;
            long    l;
        } data;
    } t_asm_opr;

    typedef struct _asm_line {
        int         type;               // Type of the assembler line
        char        *s;                 // Frame or label string
        int         opcode;             // Opcode
        int         opr_count;          // Number of operands
        t_asm_opr   **opr;              // Operand array
        int         lineno;             // Source code line number
    } t_asm_line;

    typedef struct {
        t_dll *constants;                   // represents all constant
        t_dll *identifiers;                 // represents all identifiers

        t_hash_table *label_offsets;        // Declared labels
        t_dll *backpatch_offsets;           // Label references

        int stack_size;                     // Maximum stack size needed for this frame

        int alloc_len;                      // Length currently allocated inside *bytecode
        int code_len;                       // Length of the bytecode
        char *code;                         // Actual bytecode

        int lino_len;                       // Line numbers length
        char *lino;                         // compacted line numbers
    } t_asm_frame;

    typedef struct _asm_constant {
        enum { const_string, const_long, const_code, const_regex } type;     // Constant type
        union {
            char *s;                                // String, code, regex storage
            long l;                                 // Numerical storage
        } data;
    } t_asm_constant;


    t_asm_opr *asm_create_opr(int type, char *s, int l);
    t_asm_line *asm_create_codeline(int lineno, int opcode, int opr_cnt, ...);
    t_asm_line *asm_create_frameline(char *name);
    t_asm_line *asm_create_labelline(char *label);

    t_bytecode *assembler(t_hash_table *asm_code, const char *filename);
    void assembler_free(t_hash_table *asm_code);
    void assembler_output(t_hash_table *asm_code, char *output_path);
    void assembler_output_stream(t_hash_table *asm_code, FILE *f);

#endif
