#ifndef USERMEM_H
#define USERMEM_H

#include "core/types.h"

/* User address space bounds:
 * - Kernel space: 0x00000000 - 0x0FFFFFFF (256MB, identity-mapped, shared)
 * - User space:   0x08000000 - 0xBFFFFFFF (typical ELF load area)
 * 
 * Note: User programs are typically loaded starting at 0x08000000.
 * We consider addresses >= 0x08000000 as potentially user-space.
 * Addresses < 0x08000000 are kernel space (but may be accessible by user).
 */
#define USER_SPACE_START  0x08000000
#define USER_SPACE_END    0xC0000000

/**
 * Check if a user-space address range is valid and mapped.
 * 
 * @param addr Starting address to check
 * @param len Length of the region in bytes
 * @return 1 if valid and mapped, 0 otherwise
 */
int is_user_addr_valid(u32 addr, u32 len);

/**
 * Check if a user-space null-terminated string is valid.
 * 
 * @param str Pointer to string in user space
 * @param max_len Maximum length to check (safety limit)
 * @return 1 if valid and null-terminated within max_len, 0 otherwise
 */
int is_user_string_valid(const char* str, u32 max_len);

/**
 * Safely copy data from user space to kernel space.
 * 
 * @param dst_kernel Destination buffer in kernel space
 * @param src_user Source buffer in user space
 * @param len Number of bytes to copy
 * @return 0 on success, -EFAULT on error
 */
int copy_from_user(void* dst_kernel, const void* src_user, u32 len);

/**
 * Safely copy data from kernel space to user space.
 * 
 * @param dst_user Destination buffer in user space
 * @param src_kernel Source buffer in kernel space
 * @param len Number of bytes to copy
 * @return 0 on success, -EFAULT on error
 */
int copy_to_user(void* dst_user, const void* src_kernel, u32 len);

/**
 * Safely copy a null-terminated string from user space to kernel space.
 * 
 * @param dst_kernel Destination buffer in kernel space
 * @param src_user Source string in user space
 * @param max_len Maximum length to copy (including null terminator)
 * @return Length of string (excluding null) on success, -EFAULT on error
 */
int strncpy_from_user(char* dst_kernel, const char* src_user, u32 max_len);

#endif
