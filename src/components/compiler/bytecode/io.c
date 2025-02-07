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

#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include "general/output.h"
#include "compiler/bytecode.h"
#include "general/smm.h"
#include "general/gpg.h"
#include "general/bzip2.h"
#include "general/config.h"
//#include "general/hashtable.h"
#include "compiler/output/asm.h"
#include "compiler/ast_to_asm.h"
#include "general/path_handling.h"



/**
 * Load a bytecode from disk, optionally verify signature
 */
t_bytecode *bytecode_load(const char *filename, int verify_signature) {
    t_bytecode_binary_header header;

    if (! bytecode_is_valid_file(filename)) {
        return NULL;
    }

    // Read header
    FILE *f = fopen(filename, "rb");
    if (! f) {
        fatal_error(1, "can't open file '%s'", filename);   /* LCOV_EXCL_LINE */
    }

    int rb = fread(&header, 1, sizeof(header), f);
    if (rb != sizeof(header)) {
        fatal_error(1, "can't read file '%s'", filename);   /* LCOV_EXCL_LINE */
    }

    // Allocate room and read binary code
    char *bincode = (char *)smm_malloc(header.bytecode_len);
    fseek(f, header.bytecode_offset, SEEK_SET);
    fread(bincode, header.bytecode_len, 1, f);


    // There is a signature present. Give warning when the user does not want to check it
    if (verify_signature == 0 &&
        (header.flags & BYTECODE_FLAG_SIGNED) == BYTECODE_FLAG_SIGNED &&
        header.signature_offset != 0) {
        output_char("A signature is present, but verification is disabled");
    }

    // We need to check signature, and there is one present
    if (verify_signature == 1 &&
        (header.flags & BYTECODE_FLAG_SIGNED) == BYTECODE_FLAG_SIGNED &&
        header.signature_offset != 0) {

        // Read signature
        char *signature = (char *)smm_malloc(header.signature_len);
        fseek(f, header.signature_offset, SEEK_SET);
        fread(signature, header.signature_len, 1, f);

        // Verify signature
        if (! gpg_verify(bincode, header.bytecode_len, signature, header.signature_len)) {
            fatal_error(1, "The signature for this bytecode is INVALID!");      /* LCOV_EXCL_LINE */
        }
    }

    fclose(f);

    // Uncompress bincode block
    unsigned int bzip_buf_len = header.bytecode_uncompressed_len;
    char *bzip_buf = smm_malloc(bzip_buf_len);
    if (! bzip2_decompress(bzip_buf, &bzip_buf_len, bincode, header.bytecode_len)) {
        fatal_error(1, "Error while decompressing data");       /* LCOV_EXCL_LINE */
    }

    // Sanity check. These should match
    if (bzip_buf_len != header.bytecode_uncompressed_len) {
        fatal_error(1, "Header information does not match with the size of the uncompressed data block");   /* LCOV_EXCL_LINE */
    }

    // Free unpacked binary code. We don't need it anymore
    smm_free(bincode);

    // Set bincode data to the uncompressed block
    bincode = bzip_buf;
    header.bytecode_len = bzip_buf_len;

    // Convert binary to bytecode
    t_bytecode *bc = bytecode_unmarshal(bincode);
    if (! bc) {
        fatal_error(1, "Could not convert bytecode data");  /* LCOV_EXCL_LINE */
    }

    smm_free(bzip_buf);


    // Set source filename
    char *source_path = replace_extension(filename, ".sfc", ".sf");
    bc->source_filename = realpath(source_path, NULL);
    smm_free(source_path);

    // Return bytecode
    return bc;
}


/**
 * Save a bytecode from disk, optionally sign and add signature
 */
int bytecode_save(const char *dest_filename, const char *source_filename, t_bytecode *bc) {
    char *bincode = NULL;
    int bincode_len = 0;

    // Convert bytecode to bincode
    if (! bytecode_marshal(bc, &bincode_len, &bincode)) {
        fatal_error(1, "Could not convert bytecode data");  /* LCOV_EXCL_LINE */
    }

    // Let header point to the reserved header position
    t_bytecode_binary_header header;
    bzero(&header, sizeof(t_bytecode_binary_header));

    // Set header fields
    header.magic = MAGIC_HEADER;
    header.flags = 0;

    // Fetch modification time from source file and fill into header
    struct stat sb;
    if (! stat(source_filename, &sb)) {
        header.timestamp = sb.st_mtime;
    } else {
        header.timestamp = 0;
    }

    // Save lengths of the bytecode (assume we save it uncompressed for now)
    header.bytecode_uncompressed_len = bincode_len;
    header.bytecode_len = bincode_len;


    // Compress the bincode block
    // Compress buffer
    unsigned int bzip_buf_len = 0;
    char *bzip_buf = NULL;
    if (! bzip2_compress(&bzip_buf, &bzip_buf_len, bincode, bincode_len)) {
        fatal_error(1, "Error while compressing data"); /* LCOV_EXCL_LINE */
    }

    // Forget about the original bincode and replace it with out bzip2 data.
    smm_free(bincode);
    bincode = bzip_buf;
    bincode_len = bzip_buf_len;

    // The actual bytecode binary length will differ from it's uncompressed length.
    header.bytecode_len = bzip_buf_len;

    // Create file
    FILE *f = fopen(dest_filename, "wb");
    if (f == NULL) {
        // Cannot create file
        smm_free(bincode);
        return 0;
    }

    // temporary write header
    fwrite("\0", 1, sizeof(header), f);

    // Write bytecode
    header.bytecode_offset = ftell(f);
    fwrite(bincode, bincode_len, 1, f);

    // Reset to the start of the file and write header
    fseek(f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, f);

    fclose(f);

    // Free up our binary code
    smm_free(bincode);

    return 1;
}





/**
 *
 */
int bytecode_get_timestamp(const char *path) {
    t_bytecode_binary_header header;

    // Read header
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fread(&header, sizeof(header), 1, f);
    fclose(f);

    return header.timestamp;
}

/**
 *
 */
int bytecode_is_valid_file(const char *path) {
    t_bytecode_binary_header header;

    // Read header
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fread(&header, sizeof(header), 1, f);
    fclose(f);

    return (header.magic == MAGIC_HEADER);
}


/**
 *
 */
int bytecode_is_signed(const char *path) {
    t_bytecode_binary_header header;

    // Read header
    FILE *f = fopen(path, "rb");
    fread(&header, sizeof(header), 1, f);
    fclose(f);

    return ((header.flags & BYTECODE_FLAG_SIGNED) == BYTECODE_FLAG_SIGNED &&
            header.signature_offset != 0);
}


/**
 *
 */
int bytecode_remove_signature(const char *path) {
    t_bytecode_binary_header header;

    // Sanity check
    if (! bytecode_is_signed(path)) return 1;

    // Read header
    FILE *f = fopen(path, "r+b");
    fread(&header, sizeof(header), 1, f);

    int sigpos = header.signature_offset;
    header.signature_offset = 0;
    header.signature_len = 0;
    header.flags &= ~BYTECODE_FLAG_SIGNED;

    // Write new header
    fseek(f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, f);

    // Strip away the signature (@TODO: assume signature is at end of file)
    ftruncate(fileno(f), sigpos);
    fclose(f);

    return 0;
}


/**
 * Add a new signature to the
 */
int bytecode_add_signature(const char *path, char *gpg_key) {
    t_bytecode_binary_header header;

    // Sanity check
    if (bytecode_is_signed(path)) return 1;

    // Read header
    FILE *f = fopen(path, "r+b");
    fread(&header, sizeof(header), 1, f);

    // Allocate room and read bincode from file
    char *bincode = smm_malloc(header.bytecode_len);
    fseek(f, header.bytecode_offset, SEEK_SET);
    fread(bincode, header.bytecode_len, 1, f);

    // Create signature from bincode
    char *gpg_signature = NULL;
    unsigned int gpg_signature_len = 0;
    char *_gpg_key;
    if (gpg_key == NULL) {
        _gpg_key = config_get_string("gpg.key", NULL);
    } else {
        _gpg_key = gpg_key;
    }
    if (_gpg_key == NULL) {
        fatal_error(1, "Cannot find GPG key. Please set the correct GPG key inside your INI file"); /* LCOV_EXCL_LINE */
    }
    gpg_sign(_gpg_key, bincode, header.bytecode_len, &gpg_signature, &gpg_signature_len);

    // Set new header values
    fseek(f, 0, SEEK_END);
    header.signature_offset = ftell(f);
    header.signature_len = gpg_signature_len;
    header.flags |= BYTECODE_FLAG_SIGNED;

    // Write new header
    fseek(f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, f);

    // Write signature to the end of the file (signature offset)
    fseek(f, header.signature_offset, SEEK_SET);
    fwrite(gpg_signature, gpg_signature_len, 1, f);

    fclose(f);

    return 0;
}



/**
 * Generates bytecode and tries to store to disk.
 *
 * @param source_file  The source file to compile
 * @Param bytecode_file The bytecode file to store the compiled bytecode to. If NULL, it will not try to store
 * @param success  integer to save the status code of the bytecode file saving. If NULL, no status code will be saved
 */
t_bytecode *bytecode_generate_diskfile(const char *source_file, const char *bytecode_file, int *success) {
    // (Re)generate bytecode file
    t_ast_element *ast = ast_generate_from_file(source_file);
    if (! ast) return NULL;

    t_hash_table *asm_code = ast_to_asm(ast, 1);
    if (! asm_code) {
        ast_free_node(ast);
        return NULL;
    }

    t_bytecode *bc = assembler(asm_code, source_file);
    if (! bc) {
        ast_free_node(ast);
        assembler_free(asm_code);
        return NULL;
    }

    // Save to disk if a bytecode filename is present
    if (bytecode_file) {
        int ret = bytecode_save(bytecode_file, source_file, bc);  /* This may or may not succeed. Doesn't matter */

        // Save success status from bytecode saving if needed
        if (success) {
            *success = ret;
        }
    }

    ast_free_node(ast);
    assembler_free(asm_code);

    return bc;
}
