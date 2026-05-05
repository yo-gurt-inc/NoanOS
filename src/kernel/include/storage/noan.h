#ifndef NOAN_H
#define NOAN_H

#include "core/types.h"

#define NOAN_MAGIC 0x4E4F414E // "NOAN"

typedef struct {
    u32 magic;
    u32 entry_point;    // Offset from start of binary
    u32 code_size;
    u32 data_size;
} __attribute__((packed)) noan_header_t;

/**
 * Loads a NOAN binary from /bin/name
 * Returns the memory address of the entry point on success, or 0 on failure.
 */
u32 noan_load(const char* cmd_line);

/**
 * Executes a NOAN binary at the given entry point in Ring 3.
 */
void noan_execute(u32 entry, const char* cmd_line);

#endif
