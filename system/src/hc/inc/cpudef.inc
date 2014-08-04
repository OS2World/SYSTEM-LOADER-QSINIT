;cc #ifndef QS_BASE_CPUIDINFO
;cc #define QS_BASE_CPUIDINFO

;
; CPUID & MTRR defines
;

; eax = 1, ecx bits
CPUID_FI1_SSE3        = 00000001h        ;/< Streaming SIMD Extensions 3 (SSE3)
CPUID_FI1_DTES64      = 00000004h        ;/< 64-bit DS Area
CPUID_FI1_MONITOR     = 00000008h        ;/< MONITOR/MWAIT
CPUID_FI1_DSCPL       = 00000010h        ;/< CPL Qualified Debug Store
CPUID_FI1_VMX         = 00000020h        ;/< Virtual Machine Extensions
CPUID_FI1_SMX         = 00000040h        ;/< Safer Mode Extensions
CPUID_FI1_EST         = 00000080h        ;/< SpeedStepR technology
CPUID_FI1_TM2         = 00000100h        ;/< Thermal Monitor 2
CPUID_FI1_SSSE3       = 00000200h        ;/< Supplemental SSE3
CPUID_FI1_CNXTID      = 00000400h        ;/< L1 Context ID
CPUID_FI1_CMPXCHG16B  = 00002000h        ;/< CMPXCHG16B Available
CPUID_FI1_xTPRUpdate  = 00004000h        ;/< xTPR Update Control
CPUID_FI1_PDCM        = 00008000h        ;/< Perfmon and Debug Capability
CPUID_FI1_DCA         = 00040000h        ;/< Memory mapped device prefetch
CPUID_FI1_SSE41       = 00080000h        ;/< Support of SSE4.1
CPUID_FI1_SSE42       = 00100000h        ;/< Support of SSE4.2
CPUID_FI1_x2APIC      = 00200000h        ;/< Support of x2APIC
CPUID_FI1_MOVBE       = 00400000h        ;/< MOVBE Available
CPUID_FI1_POPCNT      = 00800000h        ;/< POPCNT Available
CPUID_FI1_XSAVE       = 04000000h        ;/< XSAVE/XRSTOR, XSETBV/XGETBV Available
CPUID_FI1_OSXSAVE     = 08000000h        ;/< Enabled XSETBV/XGETBV

; eax = 1, edx bits
CPUID_FI2_FPU         = 00000001h        ;/< Floating Point Unit On-Chip
CPUID_FI2_VME         = 00000002h        ;/< Virtual 8086 Mode Enhancements
CPUID_FI2_DE          = 00000004h        ;/< Debugging Extensions
CPUID_FI2_PSE         = 00000008h        ;/< Page Size Extension
CPUID_FI2_TSC         = 00000010h        ;/< RDTSC Available
CPUID_FI2_MSR         = 00000020h        ;/< RDMSR and WRMSR Available
CPUID_FI2_PAE         = 00000040h        ;/< Physical Address Extension
CPUID_FI2_MCE         = 00000080h        ;/< Machine Check Exception
CPUID_FI2_CMPXCHG8B   = 00000100h        ;/< CMPXCHG8B Available
CPUID_FI2_APIC        = 00000200h        ;/< APIC On-Chip
CPUID_FI2_SEP         = 00000800h        ;/< SYSENTER and SYSEXIT Available
CPUID_FI2_MTRR        = 00001000h        ;/< Memory Type Range Registers
CPUID_FI2_PGE         = 00002000h        ;/< PTE Global Bit
CPUID_FI2_MCA         = 00004000h        ;/< Machine Check Architecture
CPUID_FI2_CMOV        = 00008000h        ;/< Conditional Move Instructions
CPUID_FI2_PAT         = 00010000h        ;/< Page Attribute Table is supported
CPUID_FI2_PSE36       = 00020000h        ;/< 36-Bit Page Size Extension
CPUID_FI2_PSN         = 00040000h        ;/< Processor Serial Number
CPUID_FI2_CLFSH       = 00080000h        ;/< CLFLUSH Instruction is supported
CPUID_FI2_DS          = 00200000h        ;/< Debug Store
CPUID_FI2_ACPI        = 00400000h        ;/< Thermal Monitor and Software Controlled Clock
CPUID_FI2_MMX         = 00800000h        ;/< MMX Available
CPUID_FI2_FXSR        = 01000000h        ;/< FXSAVE/FXRSTOR Available
CPUID_FI2_SSE         = 02000000h        ;/< SSE extensions available
CPUID_FI2_SSE2        = 04000000h        ;/< SSE2 extensions available
CPUID_FI2_SSNOOP      = 08000000h        ;/< Self Snoop
CPUID_FI2_HTT         = 10000000h        ;/< Multi-Threading
CPUID_FI2_TM          = 20000000h        ;/< Thermal Monitor
CPUID_FI2_PBE         = 80000000h        ;/< Pending Break Enable

; eax = 80000001h, ecx bits
CPUID_FI3_LAHF64      = 00000001h        ;/< LAHF/SAHF available in 64-bit mode

; eax = 80000001h, edx bits
CPUID_FI4_SYSCALL     = 00002000h        ;/< SYSCALL/SYSRET Available
CPUID_FI4_AMDSMP      = 00080000h        ;/< AMD SMP
CPUID_FI4_EXDBIT      = 00100000h        ;/< Execute Disable Bit available
CPUID_FI4_1GBPAGES    = 04000000h        ;/< 1Gb pages suported
CPUID_FI4_RDTSCP      = 08000000h        ;/< RDTSCP Available
CPUID_FI4_IA64        = 20000000h        ;/< IA64 Available
CPUID_FI4_3DNOWEXT    = 40000000h        ;/< 3DNow! ext
CPUID_FI4_3DNOW       = 80000000h        ;/< 3DNow!


CPU_EFLAGS_CF         = 00000001h        ;/< Carry flag
CPU_EFLAGS_PF         = 00000004h        ;/< Parity flag
CPU_EFLAGS_AF         = 00000010h        ;/< Adjust flag
CPU_EFLAGS_ZF         = 00000040h        ;/< Zero flag
CPU_EFLAGS_SF         = 00000080h        ;/< Sign flag
CPU_EFLAGS_TF         = 00000100h        ;/< Trap flag
CPU_EFLAGS_IF         = 00000200h        ;/< Interrupt enable flag
CPU_EFLAGS_DF         = 00000400h        ;/< Direction flag
CPU_EFLAGS_OF         = 00000800h        ;/< Overflow flag
CPU_EFLAGS_IOPLMASK   = 00003000h        ;/< I/O privilege level
CPU_EFLAGS_IOPLSHIFT  = 12               ;/< I/O privilege level
CPU_EFLAGS_NT         = 00004000h        ;/< Nested task flag
CPU_EFLAGS_RF         = 00010000h        ;/< Resume flag
CPU_EFLAGS_VM         = 00020000h        ;/< Virtual-8086 mode flag
CPU_EFLAGS_AC         = 00040000h        ;/< Alignment check flag
CPU_EFLAGS_VIF        = 00080000h        ;/< Virtual interrupt flag
CPU_EFLAGS_VIP        = 00100000h        ;/< Virtual interrupt pending flag
CPU_EFLAGS_CPUID      = 00200000h        ;/< Identification flag

CPU_CR0_PE            = 00000001h        ;/< protected mode flag
CPU_CR0_MP            = 00000002h        ;/< "Monitor Coprocessor" flag
CPU_CR0_EM            = 00000004h        ;/< FPU emulation flag
CPU_CR0_TS            = 00000008h        ;/< "task switched" flag
CPU_CR0_ET            = 00000010h        ;/< FPU presence flag
CPU_CR0_NE            = 00000020h        ;/< internal FPU error processing
CPU_CR0_WP            = 00010000h        ;/< write-protect flag
CPU_CR0_AM            = 00040000h        ;/< alignment checking
CPU_CR0_NW            = 20000000h        ;/< "not write-through" flag
CPU_CR0_CD            = 40000000h        ;/< cache disable flag
CPU_CR0_PG            = 80000000h        ;/< paging enabled flag

CPU_CR3_PWT           = 00000008h        ;/< pd caching method
CPU_CR3_PCD           = 00000010h        ;/< page directory caching

CPU_CR4_VME           = 00000001h        ;/< V86 mode flag
CPU_CR4_PVI           = 00000002h        ;/< virtual Interrupts flag
CPU_CR4_TSD           = 00000004h        ;/< time stamp exec.level
CPU_CR4_DE            = 00000008h        ;/< debugging extensions flag
CPU_CR4_PSE           = 00000010h        ;/< PSE enabling flag
CPU_CR4_PAE           = 00000020h        ;/< PAE enabling flag
CPU_CR4_MCE           = 00000040h        ;/< machine-check enabling flag
CPU_CR4_PGE           = 00000080h        ;/< global pages flag
CPU_CR4_PCE           = 00000100h        ;/< performance-monitoring flag
CPU_CR4_OSFXSR        = 00000200h        ;/< SSE enabling flag
CPU_CR4_OSXMMEXCPT    = 00000400h        ;/< SIMD exception support flag
CPU_CR4_VMXE          = 00002000h        ;/< VMX enabling flag
CPU_CR4_SMXE          = 00004000h        ;/< SMX enabling flag
CPU_CR4_OSXSAVE       = 00040000h        ;/< another one extended states flag

;cc #endif // QS_BASE_CPUIDINFO
