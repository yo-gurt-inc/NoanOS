#include "include/keyboard.h"
#include "include/io.h"
#include "include/idt.h"
#include "include/kprint.h"

#define KEYBOARD_DATA_PORT 0x60

static volatile int shift_held = 0;
static volatile int caps_lock = 0;
static volatile int shift_lock = 0;   // Persistent toggle
static volatile int shift_sticky = 0; // One-shot shift
static volatile int symbol_mode = 0;  // Number row always symbols
static int key_pressed_during_shift = 0;
static int debug_scancodes = 0;

void keyboard_set_debug(int enable) {
    debug_scancodes = enable;
}

void keyboard_toggle_shift_lock(void) {
    shift_lock = !shift_lock;
}

void keyboard_toggle_symbol_mode(void) {
    symbol_mode = !symbol_mode;
}

static const char scancode_ascii_nomod[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char scancode_ascii_shift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

#define BUFFER_SIZE 256
static char keyboard_buffer[BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;

u32 keyboard_handler(u32 esp) {
    u8 scancode = inb(KEYBOARD_DATA_PORT);
    
    if (debug_scancodes) {
        kprint("SC:");
        kprint_hex8(scancode);
        kprint(scancode & 0x80 ? "(UP) " : "(DN) ");
    }

    // LShift: 0x2A, RShift: 0x36
    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = 1;
        key_pressed_during_shift = 0;
        return esp;
    }
    // Release
    if (scancode == 0xAA || scancode == 0xB6) {
        if (!key_pressed_during_shift) {
            shift_sticky = 1; // Sticky!
        }
        shift_held = 0;
        return esp;
    }

    // Caps Lock: 0x3A
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return esp;
    }

    if (!(scancode & 0x80)) { // Key press
        key_pressed_during_shift = 1;
        
        if (scancode < (int)sizeof(scancode_ascii_nomod)) {
            int use_shift = shift_held || shift_lock || shift_sticky;
            
            // Symbol mode forces shift on number row
            if (symbol_mode && scancode >= 0x02 && scancode <= 0x0D) {
                use_shift = !use_shift;
            }

            char base_char = scancode_ascii_nomod[scancode];
            if (base_char >= 'a' && base_char <= 'z') {
                if (caps_lock) use_shift = !use_shift;
            }

            char c = use_shift ? scancode_ascii_shift[scancode] : scancode_ascii_nomod[scancode];
            
            if (c != 0) {
                int next = (buffer_head + 1) % BUFFER_SIZE;
                if (next != buffer_tail) {
                    keyboard_buffer[buffer_head] = c;
                    buffer_head = next;
                }
            }
            
            shift_sticky = 0; // Reset after one key
        }
    }
    return esp;
}

void keyboard_init(void) {
    shift_held = 0;
    caps_lock = 0;
    shift_lock = 0;
    shift_sticky = 0;
    symbol_mode = 0;
    irq_install_handler(1, keyboard_handler);
}

int keyboard_getchar(void) {
    if (buffer_head == buffer_tail) {
        return 0;
    }
    int c = (unsigned char)keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    return c;
}
