#include "io/keyboard.h"
#include "io/io.h"
#include "cpu/idt.h"
#include "io/kprint.h"
#include "cpu/task.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static volatile int shift_held = 0;
static volatile int ctrl_held = 0;
static volatile int caps_lock = 0;
static volatile int shift_lock = 0;
static volatile int shift_sticky = 0;
static volatile int symbol_mode = 0;
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
static volatile int buffer_head = 0;
static volatile int buffer_tail = 0;
static volatile int kb_enabled = 1;  /* 0 = discard input, 1 = buffer it */

static void push_char(char c) {
    if (!kb_enabled) return;  /* drop chars while a command is running */
    int next = (buffer_head + 1) % BUFFER_SIZE;
    if (next != buffer_tail) {
        keyboard_buffer[buffer_head] = c;
        buffer_head = next;
    }
}

u32 keyboard_handler(u32 esp) {
    u8 scancode = inb(KEYBOARD_DATA_PORT);
    
    if (debug_scancodes) {
        kprint("SC:");
        kprint_hex8(scancode);
        kprint(scancode & 0x80 ? "(UP) " : "(DN) ");
    }

    // Wake up all processes that might be waiting for keyboard input
    process_t* proc = get_process_list();
    if (proc) {
        process_t* start = proc;
        do {
            if (proc->state == TASK_SLEEPING) {
                proc->state = TASK_READY;
            }
            proc = proc->next;
        } while (proc != start);
    }

    // Modifiers
    if (scancode == 0x1D) { // Ctrl DN
        ctrl_held = 1;
        return esp;
    }
    if (scancode == 0x9D) { // Ctrl UP
        ctrl_held = 0;
        return esp;
    }
    if (scancode == 0x2A || scancode == 0x36) { // Shift DN
        shift_held = 1;
        key_pressed_during_shift = 0;
        return esp;
    }
    if (scancode == 0xAA || scancode == 0xB6) { // Shift UP
        if (!key_pressed_during_shift) shift_sticky = 1;
        shift_held = 0;
        return esp;
    }

    if (scancode == 0x3A) { // Caps Lock
        caps_lock = !caps_lock;
        return esp;
    }

    if (!(scancode & 0x80)) { // Key press
        key_pressed_during_shift = 1;

        // Arrow keys
        if (scancode == 0x48) { // Up
            push_char('\033'); push_char('['); push_char('A');
            return esp;
        }
        if (scancode == 0x50) { // Down
            push_char('\033'); push_char('['); push_char('B');
            return esp;
        }
        if (scancode == 0x4D) { // Right
            push_char('\033'); push_char('['); push_char('C');
            return esp;
        }
        if (scancode == 0x4B) { // Left
            push_char('\033'); push_char('['); push_char('D');
            return esp;
        }

        if (scancode < (int)sizeof(scancode_ascii_nomod)) {
            int use_shift = shift_held || shift_lock || shift_sticky;
            
            if (symbol_mode && scancode >= 0x02 && scancode <= 0x0D) {
                use_shift = !use_shift;
            }

            char base_char = scancode_ascii_nomod[scancode];
            
            // Handle Ctrl+Key
            if (ctrl_held && base_char >= 'a' && base_char <= 'z') {
                push_char(base_char - 'a' + 1); // Ctrl+A is 1, Ctrl+S is 19, etc.
                return esp;
            }

            if (base_char >= 'a' && base_char <= 'z') {
                if (caps_lock) use_shift = !use_shift;
            }

            char c = use_shift ? scancode_ascii_shift[scancode] : scancode_ascii_nomod[scancode];
            if (c != 0) {
                push_char(c);
            }
            shift_sticky = 0;
        }
    }
    return esp;
}

void keyboard_init(void) {
    shift_held = 0;
    ctrl_held = 0;
    caps_lock = 0;
    shift_lock = 0;
    shift_sticky = 0;
    symbol_mode = 0;
    buffer_head = 0;
    buffer_tail = 0;

    while (inb(KEYBOARD_STATUS_PORT) & 1) inb(KEYBOARD_DATA_PORT);
    irq_install_handler(1, keyboard_handler);
}

int keyboard_getchar(void) {
    if (buffer_head == buffer_tail) return 0;
    int c = (unsigned char)keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    return c;
}

void keyboard_flush(void) {
    buffer_tail = buffer_head;
    /* Reset modifier state — a flush marks a context boundary where held
     * modifiers from the previous context (e.g. Ctrl held during Ctrl+Q)
     * must not bleed into the next context. */
    shift_held = 0;
    ctrl_held = 0;
    shift_sticky = 0;
}

void keyboard_set_enabled(int enabled) {
    kb_enabled = enabled;
    /* No flush on re-enable: chars already in the buffer when the command
     * started are legitimate type-ahead and should not be discarded.
     * kb_enabled=0 already blocked any new chars during execution. */
}
