#ifndef MXCSR_SHARED_H
#define MXCSR_SHARED_H

#pragma GCC push_options
#pragma GCC optimize ("-O0")

// MXCSR Register flags as defined by Intel
#define FZ  0x8000
#define DAZ 0x40

class MXCSR {
    public:
        MXCSR() {}

        void __attribute__((noinline)) set_mxcsr_flags() {
            __asm("endbr64");
            __asm("stmxcsr -0x4(%rsp)");
            __asm("orl $0x8040,-0x4(%rsp)");
            __asm("ldmxcsr -0x4(%rsp)");
        }

        int __attribute__((noinline)) get_mxcsr_flags() {
            __asm("endbr64");
            __asm("stmxcsr -0x4(%rbp)");
            __asm("mov -0x4(%rbp),%eax");
            __asm("nop");
            __asm("pop %rbp");
            __asm("ret");
            return 0; // supresses warning
        }

        bool __attribute__((noinline)) FTZ_and_DTZ_flags_set() {
            int mxcsr_flags = get_mxcsr_flags();
            return (mxcsr_flags & FZ) && (mxcsr_flags & DAZ);
        }
};

#pragma GCC pop_options

#endif