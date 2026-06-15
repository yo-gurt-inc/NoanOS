void _start(void) {
    const char msg[] = "raw write test\n";
    __asm__ volatile("int $0x80" :: "a"(4), "b"(1), "c"(msg), "d"(15));
    __asm__ volatile("int $0x80" :: "a"(1), "b"(0));
}
