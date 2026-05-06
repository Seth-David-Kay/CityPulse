// https://github.com/felix-u 2026-02-11
// Public domain. NO WARRANTY - use at your own risk


// This library assumes it's run on a 64-bit little-endian processor.


#if !defined(OBJECT_H)
#define OBJECT_H


#if !defined(OBJECT_FUNCTION)
    #define OBJECT_FUNCTION
#endif

#if __STDC_VERSION__ >= 201112L
    #define OBJECT_STATIC_ASSERT(expression, message) _Static_assert(expression, message)
#else
    #define OBJECT_STATIC_ASSERT(expression, message)
#endif

typedef enum {
    object_Bundle_COFF = 1 << 0,
    object_Bundle_MACHO = 1 << 1,

    object_Bundle_READ = 1 << 2,
    object_Bundle_WRITE = 1 << 3,
} object_Bundle_Flags;

OBJECT_FUNCTION unsigned char *object_bundle(
    const char *array_name, unsigned array_name_length,
    const char *length_name, unsigned length_name_length,
    void *data, unsigned data_size,
    unsigned char *buffer, unsigned long long *buffer_size,
    object_Bundle_Flags flags
);

// COFF:
//      https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

typedef unsigned short object_COFF_File_Header_Flags;
enum {
    object_COFF_File_Header_RELOCS_STRIPPED         = 0x0001,
    object_COFF_File_Header_EXECUTABLE_IMAGE        = 0x0002,
    object_COFF_File_Header_LINE_NUMS_STRIPPED      = 0x0004, // deprecated, should be 0
    object_COFF_File_Header_LOCAL_SYMS_STRIPPED     = 0x0008, // deprecated, should be 0
    object_COFF_File_Header_AGGRESSIVE_WS_TRIM      = 0x0010, // deprecated, should be 0
    object_COFF_File_Header_LARGE_ADDRESS_AWARE     = 0x0020,
    object_COFF_File_Header_BYTES_REVERSED_LO       = 0x0080, // deprecated, should be 0
    object_COFF_File_Header_32BIT_MACHINE           = 0x0100,
    object_COFF_File_Header_DEBUG_STRIPPED          = 0x0200,
    object_COFF_File_Header_REMOVABLE_RUN_FROM_SWAP = 0x0400,
    object_COFF_File_Header_NET_RUN_FROM_SWAP       = 0x0800,
    object_COFF_File_Header_SYSTEM                  = 0x1000,
    object_COFF_File_Header_DLL                     = 0x2000,
    object_COFF_File_Header_UP_SYSTEM_ONLY          = 0x4000,
    object_COFF_File_Header_BYTES_REVERSED_HI       = 0x8000, // deprecated, should be 0
};

typedef struct {
    unsigned short machine; // also given as "magic"
    unsigned short number_of_sections;
    unsigned time_date_stamp;
    unsigned pointer_to_symbol_table;
    unsigned number_of_symbols;
    unsigned short size_of_optional_header;
    object_COFF_File_Header_Flags flags;
} object_COFF_File_Header;
OBJECT_STATIC_ASSERT(sizeof(object_COFF_File_Header) == 20, "code assumes no added padding");

typedef unsigned object_COFF_Section_Header_Flags;
enum {
    object_COFF_Section_Header_TYPE_NO_PAD            = 0x00000008, // deprecated, use ALIGN_1BYTES
    object_COFF_Section_Header_CNT_CODE               = 0x00000020,
    object_COFF_Section_Header_CNT_INITIALIZED_DATA   = 0x00000040,
    object_COFF_Section_Header_CNT_UNINITIALIZED_DATA = 0x00000080,
    object_COFF_Section_Header_LNK_OTHER              = 0x00000100, // reserved for future use
    object_COFF_Section_Header_LNK_INFO               = 0x00000200,
    object_COFF_Section_Header_LNK_REMOVE             = 0x00000800,
    object_COFF_Section_Header_LNK_COMDAT             = 0x00001000,
    object_COFF_Section_Header_GPREL                  = 0x00008000,
    object_COFF_Section_Header_MEM_PURGEABLE          = 0x00020000, // reserved for future use
    object_COFF_Section_Header_MEM_16BIT              = 0x00020000, // reserved for future use
    object_COFF_Section_Header_MEM_LOCKED             = 0x00040000, // reserved for future use
    object_COFF_Section_Header_MEM_PRELOAD            = 0x00080000, // reserved for future use
    object_COFF_Section_Header_ALIGN_1BYTES           = 0x00100000,
    object_COFF_Section_Header_ALIGN_2BYTES           = 0x00200000,
    object_COFF_Section_Header_ALIGN_4BYTES           = 0x00300000,
    object_COFF_Section_Header_ALIGN_8BYTES           = 0x00400000,
    object_COFF_Section_Header_ALIGN_16BYTES          = 0x00500000,
    object_COFF_Section_Header_ALIGN_32BYTES          = 0x00600000,
    object_COFF_Section_Header_ALIGN_64BYTES          = 0x00700000,
    object_COFF_Section_Header_ALIGN_128BYTES         = 0x00800000,
    object_COFF_Section_Header_ALIGN_256BYTES         = 0x00900000,
    object_COFF_Section_Header_ALIGN_512BYTES         = 0x00A00000,
    object_COFF_Section_Header_ALIGN_1024BYTES        = 0x00B00000,
    object_COFF_Section_Header_ALIGN_2048BYTES        = 0x00C00000,
    object_COFF_Section_Header_ALIGN_4096BYTES        = 0x00D00000,
    object_COFF_Section_Header_ALIGN_8192BYTES        = 0x00E00000,
    object_COFF_Section_Header_LNK_NRELOC_OVFL        = 0x01000000,
    object_COFF_Section_Header_MEM_DISCARDABLE        = 0x02000000,
    object_COFF_Section_Header_MEM_NOT_CACHED         = 0x04000000,
    object_COFF_Section_Header_MEM_NOT_PAGED          = 0x08000000,
    object_COFF_Section_Header_MEM_SHARED             = 0x10000000,
    object_COFF_Section_Header_MEM_EXECUTE            = 0x20000000,
    object_COFF_Section_Header_MEM_READ               = 0x40000000,
    // NOTE(felix): this bypasses error "enumerator value is not representable in the underlying type". I suppose the anonymous enum is treated as a signed int while parsing
    #define object_COFF_Section_Header_MEM_WRITE 0x80000000
};

typedef struct {
    char name[8];
    unsigned virtual_size;
    unsigned virtual_address;
    unsigned size_of_raw_data;
    unsigned pointer_to_raw_data;
    unsigned pointer_to_relocations;
    unsigned pointer_to_line_numbers;
    unsigned short number_of_relocations;
    unsigned short number_of_line_numbers;
    object_COFF_Section_Header_Flags flags;
} object_COFF_Section_Header;
OBJECT_STATIC_ASSERT(sizeof(object_COFF_Section_Header) == 40, "code assumes no added padding");

typedef union {
    char short_name[8];
    struct { unsigned zeroes, offset; } string_table;
} object_COFF_Symbol_Name;

// docs: "Microsoft tools rely on Visual C++ debug format for most symbolic information and generally use only four storage-class values: EXTERNAL (2), STATIC (3), FUNCTION (101), and STATIC (103)"
typedef unsigned char object_COFF_Symbol_Storage_Class;
enum {
    object_COFF_Symbol_Storage_Class_END_OF_FUNCTION     = 255,
    object_COFF_Symbol_Storage_Class_NULL                =   0,
    object_COFF_Symbol_Storage_Class_AUTOMATIC           =   1,
    object_COFF_Symbol_Storage_Class_EXTERNAL            =   2, // symbol.value = (symbol.section_number == 0) ? size : offset within section
    object_COFF_Symbol_Storage_Class_STATIC              =   3, // symbol.value = symbol offset within section
    object_COFF_Symbol_Storage_Class_REGISTER            =   4,
    object_COFF_Symbol_Storage_Class_EXTERNAL_DEF        =   5,
    object_COFF_Symbol_Storage_Class_LABEL               =   6,
    object_COFF_Symbol_Storage_Class_UNDEFINED_LABEL     =   7,
    object_COFF_Symbol_Storage_Class_MEMBER_OF_STRUCT    =   8,
    object_COFF_Symbol_Storage_Class_ARGUMENT            =   9,
    object_COFF_Symbol_Storage_Class_STRUCT_TAG          =  10,
    object_COFF_Symbol_Storage_Class_MEMBER_OF_UNION     =  11,
    object_COFF_Symbol_Storage_Class_UNION_TAG           =  12,
    object_COFF_Symbol_Storage_Class_TYPE_DEFINITION     =  13,
    object_COFF_Symbol_Storage_Class_UNDEFINED_STATIC    =  14,
    object_COFF_Symbol_Storage_Class_ENUM_TAG            =  15,
    object_COFF_Symbol_Storage_Class_MEMBER_OF_ENUM      =  16,
    object_COFF_Symbol_Storage_Class_REGISTER_PARAM      =  17,
    object_COFF_Symbol_Storage_Class_BIT_FIELD           =  18,
    object_COFF_Symbol_Storage_Class_BLOCK               = 100,
    object_COFF_Symbol_Storage_Class_FUNCTION            = 101,
    object_COFF_Symbol_Storage_Class_END_OF_STRUCT       = 102,
    object_COFF_Symbol_Storage_Class_CLASS_FILE          = 103,
    object_COFF_Symbol_Storage_Class_CLASS_SECTION       = 104,
    object_COFF_Symbol_Storage_Class_CLASS_WEAK_EXTERNAL = 105,
    object_COFF_Symbol_Storage_Class_CLASS_CLR_TOKEN     = 107,
};

typedef short object_COFF_Symbol_Section_Number;
enum {
    object_COFF_Symbol_Section_Number_UNDEFINED = 0,
    object_COFF_Symbol_Section_Number_ABSOLUTE = -1,
    object_COFF_Symbol_Section_Number_DEBUG = -2,
};

typedef unsigned char object_COFF_Symbol_Component_Type;
enum {
    // docs: "Microsoft tools generally do not use this field and set the LSB to 0"
    object_COFF_Symbol_Base_Type_NULL   =  0, // none or unknown
    object_COFF_Symbol_Base_Type_VOID   =  1, // no valid type; void pointers or functions
    object_COFF_Symbol_Base_Type_CHAR   =  2,
    object_COFF_Symbol_Base_Type_SHORT  =  3,
    object_COFF_Symbol_Base_Type_INT    =  4,
    object_COFF_Symbol_Base_Type_LONG   =  5,
    object_COFF_Symbol_Base_Type_FLOAT  =  6,
    object_COFF_Symbol_Base_Type_DOUBLE =  7,
    object_COFF_Symbol_Base_Type_STRUCT =  8,
    object_COFF_Symbol_Base_Type_UNION  =  9,
    object_COFF_Symbol_Base_Type_ENUM   = 10,
    object_COFF_Symbol_Base_Type_MOE    = 11, // member of enum
    object_COFF_Symbol_Base_Type_BYTE   = 12,
    object_COFF_Symbol_Base_Type_WORD   = 13,
    object_COFF_Symbol_Base_Type_UINT   = 14,
    object_COFF_Symbol_Base_Type_DWORD  = 15,

    // docs: "Microsoft tools use this field only to indicate whether the symbol is a function, so that the only two resulting values are 0x0 and 0x20 for the Type field. However, other tools can use this field to communicate more information."
    object_COFF_Symbol_Complex_Type_NULL     = 0, // no derived type; simple scalar variable
    object_COFF_Symbol_Complex_Type_POINTER  = 1,
    object_COFF_Symbol_Complex_Type_FUNCTION = 2,
    object_COFF_Symbol_Complex_Type_ARRAY    = 3,
};
typedef struct {
    object_COFF_Symbol_Component_Type complex;
    object_COFF_Symbol_Component_Type base;
} object_COFF_Symbol_Type;

typedef struct {
    object_COFF_Symbol_Name name;
    unsigned value;
    object_COFF_Symbol_Section_Number section_number;
    object_COFF_Symbol_Type type;
    object_COFF_Symbol_Storage_Class storage_class;
    unsigned char number_of_aux_symbols;
} object_COFF_Symbol;
#define OBJECT_COFF_SYMBOL_SIZE 18 // for this one, memcpy by this, not sizeof!
OBJECT_STATIC_ASSERT(sizeof(object_COFF_Symbol) == OBJECT_COFF_SYMBOL_SIZE + 2, "code assumes 2 added trailing padding bytes");

// MachO:
//      https://en.wikipedia.org/wiki/Mach-O
//      https://github.com/aidansteele/osx-abi-macho-file-format-reference

typedef unsigned object_MachO_CPU_Type;
enum {
    object_MachO_CPU_Type_ARCH_ABI64      = 0x01000000,
    object_MachO_CPU_Type_ARCH_ABI64_32   = 0x02000000,
    #define object_MachO_CPU_Type_ARCH_MASK 0xff000000

    object_MachO_CPU_Type_VAX                = 0x00000001,
    object_MachO_CPU_Type_ROMP               = 0x00000002,
    object_MachO_CPU_Type_NS32032            = 0x00000004,
    object_MachO_CPU_Type_NS32332            = 0x00000005,
    object_MachO_CPU_Type_MC680X0            = 0x00000006,
    object_MachO_CPU_Type_X86                = 0x00000007,
    object_MachO_CPU_Type_MIPS               = 0x00000008,
    object_MachO_CPU_Type_NS32352            = 0x00000009,
    object_MachO_CPU_Type_HP_PA              = 0x0000000b,
    object_MachO_CPU_Type_ARM                = 0x0000000c,
    object_MachO_CPU_Type_MC88000            = 0x0000000d,
    object_MachO_CPU_Type_SPARC              = 0x0000000e,
    object_MachO_CPU_Type_I860_BIG_ENDIAN    = 0x0000000f,
    object_MachO_CPU_Type_I860_LITTLE_ENDIAN = 0x00000010,
    object_MachO_CPU_Type_RS6000             = 0x00000011,
    object_MachO_CPU_Type_POWERPC            = 0x00000012,
    object_MachO_CPU_Type_RISCV              = 0x00000018,

    object_MachO_CPU_Type_ARM64 = object_MachO_CPU_Type_ARM | object_MachO_CPU_Type_ARCH_ABI64,
};

typedef unsigned object_MachO_CPU_Subtype;
enum {
    #define object_MachO_CPU_Subtype_LIB64 0x80000000

    object_MachO_CPU_Subtype_ARM_ALL       = 0x00,
    object_MachO_CPU_Subtype_ARM_A500_ARCH = 0x01,
    object_MachO_CPU_Subtype_ARM_A500      = 0x02,
    object_MachO_CPU_Subtype_ARM_A440      = 0x03,
    object_MachO_CPU_Subtype_ARM_M4        = 0x04,
    object_MachO_CPU_Subtype_ARM_V4T       = 0x05,
    object_MachO_CPU_Subtype_ARM_V6        = 0x06,
    object_MachO_CPU_Subtype_ARM_V5TEJ     = 0x07,
    object_MachO_CPU_Subtype_ARM_XSCALE    = 0x08,
    object_MachO_CPU_Subtype_ARM_V7        = 0x09,
    object_MachO_CPU_Subtype_ARM_V7F       = 0x0a,
    object_MachO_CPU_Subtype_ARM_V7S       = 0x0b,
    object_MachO_CPU_Subtype_ARM_V7K       = 0x0c,
    object_MachO_CPU_Subtype_ARM_V8        = 0x0d,
    object_MachO_CPU_Subtype_ARM_V6M       = 0x0e,
    object_MachO_CPU_Subtype_ARM_V7M       = 0x0f,
    object_MachO_CPU_Subtype_ARM_V7EM      = 0x10,

    object_MachO_CPU_Subtype_X86_ALL            = 0x03,
    object_MachO_CPU_Subtype_X86_486            = 0x04,
    object_MachO_CPU_Subtype_X86_486SX          = 0x84,
    object_MachO_CPU_Subtype_X86_PENTIUM_M5     = 0x56,
    object_MachO_CPU_Subtype_X86_CELERON        = 0x67,
    object_MachO_CPU_Subtype_X86_CELERON_MOBILE = 0x77,
    object_MachO_CPU_Subtype_X86_PENTIUM_3      = 0x08,
    object_MachO_CPU_Subtype_X86_PENTIUM_3_M    = 0x18,
    object_MachO_CPU_Subtype_X86_PENTIUM_3_XEON = 0x28,
    object_MachO_CPU_Subtype_X86_PENTIUM_4      = 0x0a,
    object_MachO_CPU_Subtype_X86_ITANIUM        = 0x0b,
    object_MachO_CPU_Subtype_X86_ITANIUM_2      = 0x1b,
    object_MachO_CPU_Subtype_X86_XEON           = 0x0c,
    object_MachO_CPU_Subtype_X86_XEON_MP        = 0x1c,
};

typedef unsigned object_MachO_File_Type;
enum {
    object_MachO_File_Type_RELOCATABLE_OBJECT                          = 0x01,
    object_MachO_File_Type_DEMAND_PAGED_EXECUTABLE                     = 0x02,
    object_MachO_File_Type_FIXED_VM_SHARED_LIBRARY                     = 0x03,
    object_MachO_File_Type_CORE                                        = 0x04,
    object_MachO_File_Type_PRELOADED_EXECUTABLE                        = 0x05,
    object_MachO_File_Type_DYNAMICALLY_BOUND_SHARED_LIBRARY            = 0x06,
    object_MachO_File_Type_DYNAMIC_LINK_EDITOR                         = 0x07,
    object_MachO_File_Type_DYNAMICALLY_BOUND_BUNDLE                    = 0x08,
    object_MachO_File_Type_SHARED_LIBRARY_STUB_FOR_STATIC_LINKING_ONLY = 0x09,
    object_MachO_File_Type_COMPANION_WITH_ONLY_DEBUG_SECTIONS          = 0x0a,
    object_MachO_File_Type_X86_64_KEXTS                                = 0x0b,
    object_MachO_File_Type_MACHOS_SHARING_SINGLE_LINKEDIT              = 0x0c,
};

typedef unsigned object_MachO_File_Flags;
enum {
    object_MachO_File_HAS_NO_UNDEFINED_REFERENCES                          = 1 <<  0,
    object_MachO_File_IS_INCREMENTAL_LINK_OUTPUT_CANT_LINK_EDIT            = 1 <<  1,
    object_MachO_File_IS_DYNAMIC_LINK_INPUT_CANT_STATICALLY_LINK_EDIT      = 1 <<  2,
    object_MachO_File_UNDEFINED_REFERENCES_BOUND_BY_DYNAMIC_LINKER_AT_LOAD = 1 <<  3,
    object_MachO_File_DYNAMIC_UNDEFINED_REFERENCES_PREBOUND                = 1 <<  4,
    object_MachO_File_HAS_SPLIT_READ_ONLY_AND_READ_WRITE_SEGMENTS          = 1 <<  5,
    object_MachO_File_LAZY_SHARED_LIB_INIT_ROUTINE                         = 1 <<  6, // obsolete
    object_MachO_File_2_LEVEL_NAMESPACE_BINDINGS                           = 1 <<  7,
    object_MachO_File_FORCE_FLAT_NAMESPACE_BINDINGS                        = 1 <<  8,
    object_MachO_File_NO_MULTI_SYMBOL_DEFINITIONS                          = 1 <<  9,
    object_MachO_File_DYLD_DONT_NOTIFY_PREBINDING_AGENT                    = 1 << 10,
    object_MachO_File_NOT_PREBOUND                                         = 1 << 11,
    object_MachO_File_BINDS_ALL_2_LEVEL_NAMESPACE_MODULES_OF_DEPENDENTS    = 1 << 12,
    object_MachO_File_SAFE_TO_SUBDIVIDE_SECTIONS_FOR_DEAD_CODE_STRIPPING   = 1 << 13,
    object_MachO_File_CANONICALIZED_VIA_UNPREBIND                          = 1 << 14,
    object_MachO_File_CONTAINS_EXTERNAL_WEAK_SYMBOLS                       = 1 << 15,
    object_MachO_File_USES_WEAK_SYMBOLS                                    = 1 << 16,
    object_MachO_File_ALL_STACKS_HAVE_STACK_EXECUTION_PRIVILEGE            = 1 << 17,
    object_MachO_File_SAFE_TO_USE_WITH_UID_0                               = 1 << 18,
    object_MachO_File_SAFE_TO_USE_WITH_UGID_TRUE                           = 1 << 19,
    object_MachO_File_DYLIB_LINKER_IGNORE_DEPENDENT_DYLIB_REEXPORTS        = 1 << 20,
    object_MachO_File_LOAD_AT_RANDOM_ADDRESS                               = 1 << 21,
    object_MachO_File_DYLIB_LINKER_LAZY_LOAD_COMMAND_CREATION              = 1 << 22,
    object_MachO_File_CONTAINS_S_THREAD_LOCAL_VARIABLES_SECTION            = 1 << 23,
    object_MachO_File_ALWAYS_RUN_WITH_NON_EXECUTABLE_HEAP                  = 1 << 24,
    object_MachO_File_CODE_LINKED_FOR_APP                                  = 1 << 25,
    object_MachO_File_NLIST_TABLE_OMITS_SOME_SYMBOLS_IN_DYLD_INFO          = 1 << 26,
    object_MachO_File_ALLOW_SPECIAL_LOAD_COMMANDS_ON_OTHER_OS              = 1 << 27,
    #define object_MachO_File_DYLIB_PART_OF_DYLD_SHARED_CACHE               (1 << 31)
};

typedef unsigned object_MachO_File_Header_Magic;
#define object_MachO_File_Header_Magic_LITTLE_ENDIAN_32_BIT 0xfeedface
#define object_MachO_File_Header_Magic_LITTLE_ENDIAN_64_BIT 0xfeedfacf
#define object_MachO_File_Header_Magic_BIG_ENDIAN_32_BIT 0xcefaedfe
#define object_MachO_File_Header_Magic_BIG_ENDIAN_64_BIT 0xcffaedfe

typedef struct {
    object_MachO_File_Header_Magic magic;
    object_MachO_CPU_Type cpu_type;
    object_MachO_CPU_Subtype cpu_subtype;
    object_MachO_File_Type file_type;
    unsigned number_of_load_commands;
    unsigned size_of_load_commands;
    object_MachO_File_Flags flags;
    unsigned reserved_;
} object_MachO_File_Header;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_File_Header) == 32, "code assumes no added padding");

typedef unsigned object_MachO_Load_Command_Type;
enum {
    object_MachO_Load_Command_Type_SEGMENT_32_BIT    = 0x01,
    object_MachO_Load_Command_Type_SYMBOL_TABLE      = 0x02,
    object_MachO_Load_Command_Type_SYMBOL_TABLE_INFO = 0x0b,
    object_MachO_Load_Command_Type_SEGMENT_64_BIT    = 0x19,
    object_MachO_Load_Command_Type_MIN_OS_VERSION    = 0x32,
    #define object_MachO_Load_Command_Type_NECESSARY 0x80000000 // bit flag, #defined because "unrepresentable in underlying type"
};

typedef unsigned object_MachO_Memory_Permissions;
enum {
    object_MachO_Memory_Permission_READ    = 0x01,
    object_MachO_Memory_Permission_WRITE   = 0x02,
    object_MachO_Memory_Permission_EXECUTE = 0x04,
};

typedef unsigned object_MachO_Segment_Flags;
enum {
    object_MachO_Segment_Flag_CONTENTS_HIGH_LOW_ZERO       = 0x00001, // for stacks in core files
    object_MachO_Segment_Flag_VM_ALLOCATED_BY_FIXED_VM_LIB = 0x00010,
    object_MachO_Segment_Flag_NO_RELOCATION                = 0x00100,
    object_MachO_Segment_Flag_PROTECTED                    = 0x01000,
    object_MachO_Segment_Flag_READ_ONLY_AFTER_RELOCATIONS  = 0x10000,
};

typedef struct {
    object_MachO_Load_Command_Type type;
    unsigned command_size;
    char segment_name[16];
    unsigned long long address;
    unsigned long long address_size;
    unsigned long long file_offset;
    unsigned long long size; // bytes from file offset
    object_MachO_Memory_Permissions maximum_memory_protections;
    object_MachO_Memory_Permissions initial_virtual_memory_protections;
    unsigned number_of_sections;
    object_MachO_Segment_Flags flag32;
} object_MachO_Load_Command_Segment_64_Bit;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Load_Command_Segment_64_Bit) == 72, "code assumes no added padding");

typedef unsigned object_MachO_Segment_Section_Flag_Or_Type;
enum {
    #define object_MachO_Segment_Section_Flag_ONLY_MACHINE_INSTRUCTIONS    (1 << 31)
    object_MachO_Segment_Section_Flag_CONTAINS_COALESCED_SYMBOLS          = 1 << 30,
    object_MachO_Segment_Section_Flag_STRIP_STATIC_SYMBOLS_IF_MH_DYLDLINK = 1 << 29,
    object_MachO_Segment_Section_Flag_NO_DEAD_STRIPPING                   = 1 << 28,
    object_MachO_Segment_Section_Flag_LIVE_BLOCKS_CONTAGIOUS              = 1 << 27,
    object_MachO_Segment_Section_Flag_I386_CODE_STUB                      = 1 << 26,
    object_MachO_Segment_Section_Flag_DEBUG                               = 1 << 25,
    object_MachO_Segment_Section_Flag_SOME_MACHINE_INSTRUCTIONS           = 1 << 10,
    object_MachO_Segment_Section_Flag_HAS_EXTERNAL_RELOCATIONS            = 1 <<  9,
    object_MachO_Segment_Section_Flag_HAS_LOCAL_RELOCATIONS               = 1 <<  8,

    object_MachO_Segment_Section_Type_ONLY_NON_LAZY_SYMBOL_POINTERS                   = 0x06,
    object_MachO_Segment_Section_Type_ONLY_LAZY_SYMBOL_POINTERS                       = 0x07,
    object_MachO_Segment_Section_Type_ONLY_SYMBOL_STUBS                               = 0x08,
    object_MachO_Segment_Section_Type_ZERO_FILL_ON_DEMAND                             = 0x0c,
    object_MachO_Segment_Section_Type_ONLY_LAZY_SYMBOL_POINTERS_TO_LAZY_LOADED_DYLIBS = 0x10,
};

typedef struct {
    char name[16];
    char segment_name[16];
    unsigned long long address;
    unsigned long long size;
    unsigned file_offset;
    unsigned alignment_power_of_2;
    unsigned relocations_file_offset;
    unsigned number_of_relocations;
    object_MachO_Segment_Section_Flag_Or_Type flag_or_type;
    unsigned reserved1_, reserved2_, reserved3_;
} object_MachO_Segment_Section_64_Bit;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Segment_Section_64_Bit) == 80, "code assumes no added padding");

typedef struct {
    object_MachO_Load_Command_Type type;
    unsigned command_size;
    unsigned symbols_file_offset;
    unsigned number_of_symbols;
    unsigned string_table_file_offset;
    unsigned string_table_size;
} object_MachO_Load_Command_Symbol_Table;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Load_Command_Symbol_Table) == 24, "code assumes no added padding");

typedef struct {
    object_MachO_Load_Command_Type type;
    unsigned command_size;
    unsigned local_symbol_index;
    unsigned number_of_local_symbols;
    unsigned external_symbols_index;
    unsigned number_of_external_symbols;
    unsigned undefined_symbols_index;
    unsigned number_of_undefined_symbols;
    unsigned content_table_offset;
    unsigned number_of_content_table_entries;
    unsigned module_table_offset;
    unsigned number_of_module_table_entries;
    unsigned referenced_symbol_table_offset;
    unsigned number_of_referenced_symbol_table_entries;
    unsigned indirect_symbol_table_offset;
    unsigned number_of_indirect_symbol_table_entries;
    unsigned external_relocation_offset;
    unsigned number_of_external_relocation_entries;
    unsigned local_relocation_offset;
    unsigned number_of_local_relocation_entries;
} object_MachO_Load_Command_Symbol_Table_Info;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Load_Command_Symbol_Table_Info) == 80, "code assumes no added padding");

typedef unsigned object_MachO_Platform_Type;
enum {
    object_MachO_Platform_MACOS              = 0x01,
    object_MachO_Platform_IOS                = 0x02,
    object_MachO_Platform_TVOS               = 0x03,
    object_MachO_Platform_WATCHOS            = 0x04,
    object_MachO_Platform_BRIDGEOS           = 0x05,
    object_MachO_Platform_MAC_CATALYST       = 0x06,
    object_MachO_Platform_IOS_SIMULATOR      = 0x07,
    object_MachO_Platform_TVOS_SIMULATOR     = 0x08,
    object_MachO_Platform_WATCHOS_SIMULATOR  = 0x09,
    object_MachO_Platform_DRIVERKIT          = 0x0a,
    object_MachO_Platform_VISIONOS           = 0x0b,
    object_MachO_Platform_VISIONOS_SIMULATOR = 0x0c,
};

typedef struct {
    object_MachO_Load_Command_Type type;
    unsigned command_size;
    object_MachO_Platform_Type platform_type;
    unsigned min_os_version;
    unsigned sdk_version;
    unsigned number_of_tools_used;
} object_MachO_Load_Command_Min_OS_Version;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Load_Command_Min_OS_Version) == 24, "code assumes no added padding");

typedef unsigned char object_MachO_Symbol_Type;
enum {
    object_MachO_Symbol_Type_UNDEFINED                 = 0x00,
    object_MachO_Symbol_Type_ABSOLUTE                  = 0x02,
    object_MachO_Symbol_Type_INDIRECT                  = 0x0a,
    object_MachO_Symbol_Type_PREBOUND_UNDEFINED        = 0x0c,
    object_MachO_Symbol_Type_DEFINED_IN_SECTION_NUMBER = 0x0e,

    object_MachO_Symbol_Type_PRIVATE  = 0x10,
    object_MachO_Symbol_Type_EXTERNAL = 0x01,
};

typedef unsigned char object_MachO_Symbol_Dynamic_Loader_Flags_And_Address_Type;
enum {
    object_MachO_Symbol_Dynamic_Loader_Flag_SYMBOL_DEFINED_AND_REFERENCED     = 0x10,
    object_MachO_Symbol_Dynamic_Loader_Flag_USED_BY_DYNAMIC_LINKER_AT_RUNTIME = 0x20,
    object_MachO_Symbol_Dynamic_Loader_Flag_ADDRESS_0_IF_UNDEFINED            = 0x40,
    object_MachO_Symbol_Dynamic_Loader_Flag_IGNORE_DEFINITION_IF_REPEATED     = 0x80,

    object_MachO_Symbol_Address_Type_NON_LAZY_METHOD_CALL             = 0x00,
    object_MachO_Symbol_Address_Type_LAZY_METHOD_CALL                 = 0x01,
    object_MachO_Symbol_Address_Type_METHOD_CALL_DEFINED_HERE         = 0x02,
    object_MachO_Symbol_Address_Type_PRIVATE_METHOD_CALL_DEFINED_HERE = 0x03,
    object_MachO_Symbol_Address_Type_PRIVATE_NON_LAZY_METHOD_CALL     = 0x04,
    object_MachO_Symbol_Address_Type_PRIVATE_LAZY_METHOD_CALL         = 0x05,
};

typedef struct {
    unsigned char library_ordinal_number;
    object_MachO_Symbol_Dynamic_Loader_Flags_And_Address_Type dynamic_loader_flags_and_address_type;
} object_MachO_Symbol_Data_Info;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Symbol_Data_Info) == 2, "code assumes no added padding");

typedef struct {
    unsigned name_offset;
    object_MachO_Symbol_Type type;
    unsigned char section_number;
    object_MachO_Symbol_Data_Info data_info;
    unsigned long long address;
} object_MachO_Symbol_64_Bit;
OBJECT_STATIC_ASSERT(sizeof(object_MachO_Symbol_64_Bit) == 16, "code assumes no added padding");


#endif // OBJECT_H


#if defined(OBJECT_IMPLEMENTATION)


#if !defined(OBJECT_ASSERT)
    #include <assert.h>
    #define OBJECT_ASSERT assert
#endif

#if !defined(OBJECT_MEMCPY)
    #define OBJECT_MEMCPY object__memcpy
    static inline void *object__memcpy(void *destination, const void *source, unsigned long long count) {
        for (unsigned long long i = 0; i < count; i += 1) ((char *)destination)[i] = ((const char *)source)[i];
        return destination;
    }
#endif

static inline void object__write(unsigned char *buffer, unsigned long long *written, const void *data, unsigned long long size) {
    OBJECT_MEMCPY(buffer + *written, data, size);
    *written += size;
}

static inline unsigned object__macho_version(unsigned short major, unsigned char minor, unsigned char patch) {
    unsigned result = ((unsigned)major << 16) | ((unsigned)minor << 8) | (unsigned)patch;
    return result;
}

OBJECT_FUNCTION unsigned char *object_bundle(
    const char *array_name, unsigned array_name_length,
    const char *length_name, unsigned length_name_length,
    void *data, unsigned data_size,
    unsigned char *buffer, unsigned long long *buffer_size,
    object_Bundle_Flags flags
) {
    unsigned char *data_in_object_file = 0;
    unsigned long long written = 0;

    unsigned long long data_size_without_length = (unsigned long long)data_size;
    data_size += sizeof(unsigned long long); // for length variable

    _Bool read = !!(flags & object_Bundle_READ);
    _Bool write = !!(flags & object_Bundle_WRITE);

    if (flags & object_Bundle_COFF) {
        OBJECT_ASSERT(!(flags & object_Bundle_MACHO));

        unsigned long long size_of_headers = sizeof(object_COFF_File_Header) + sizeof(object_COFF_Section_Header);

        // docs: "For object files, the value should be aligned on a 4-byte boundary for best performance."
        unsigned long long align_padding = (4 - (size_of_headers & 3)) & 3;

        _Bool array_name_is_in_string_table = array_name_length > 8;
        _Bool length_name_is_in_string_table = length_name_length > 8;
        unsigned string_table_size = sizeof string_table_size;
        string_table_size += (unsigned)array_name_is_in_string_table * (array_name_length + 1);
        string_table_size += (unsigned)length_name_is_in_string_table * (length_name_length + 1);

        unsigned long long bytes_needed = size_of_headers + align_padding + (unsigned long long)data_size;
        bytes_needed += 2 * sizeof(object_COFF_Symbol);
        bytes_needed += (unsigned long long)string_table_size;

        OBJECT_ASSERT(bytes_needed <= 0xffffffff && "COFF file offsets are 32-bit");

        if (buffer == 0 || *buffer_size < bytes_needed) {
            *buffer_size = bytes_needed;
            return 0;
        }

        object_COFF_File_Header file_header = {
            .machine = 0, // unknown / applicable to any
            .number_of_sections = 1,
            .time_date_stamp = 0, // none
            .pointer_to_symbol_table = (unsigned)(size_of_headers + align_padding) + data_size,
            .number_of_symbols = 2,
            .size_of_optional_header = 0, // should be zero for object file
            .flags = object_COFF_File_Header_LARGE_ADDRESS_AWARE | object_COFF_File_Header_DEBUG_STRIPPED,
        };
        object__write(buffer, &written, &file_header, sizeof file_header);

        object_COFF_Section_Header section_header = {
            .virtual_size = 0, // should be 0 for object files
            .virtual_address = 0, // "for simplicity, compilers should set this to zero"
            .size_of_raw_data = data_size,
            .pointer_to_raw_data = (unsigned)(written + sizeof section_header + align_padding),
            .pointer_to_relocations = 0, // none
            .pointer_to_line_numbers = 0, // none
            .number_of_relocations = 0,
            .number_of_line_numbers = 0,
        };
        {
            if (write) OBJECT_MEMCPY(section_header.name, ".data", sizeof ".data" - 1);
            else {
                OBJECT_ASSERT(read);
                OBJECT_MEMCPY(section_header.name, ".rdata", sizeof ".rdata" - 1);
            }

            section_header.flags = object_COFF_Section_Header_CNT_INITIALIZED_DATA;
            section_header.flags |= read * object_COFF_Section_Header_MEM_READ;
            section_header.flags |= write * object_COFF_Section_Header_MEM_WRITE;

            object__write(buffer, &written, &section_header, sizeof section_header);
        }

        object__write(buffer, &written, "\0\0\0", align_padding);
        OBJECT_ASSERT((unsigned)written == section_header.pointer_to_raw_data);
        {
            object__write(buffer, &written, &data_size_without_length, sizeof data_size_without_length);
            data_in_object_file = &buffer[written];

            // if data == 0, user will write
            if (data != 0) OBJECT_MEMCPY(buffer + written, data, data_size_without_length);
            written += data_size_without_length;
        }
        OBJECT_ASSERT((unsigned)written == file_header.pointer_to_symbol_table);

        { // size of data
            object_COFF_Symbol symbol = {
                .value = 0, // offset in section header data
                .section_number = 1, // one-based index
                .type = {
                    .complex = object_COFF_Symbol_Complex_Type_NULL,
                    .base = object_COFF_Symbol_Base_Type_NULL, // no such thing as u64?
                },
                .storage_class = object_COFF_Symbol_Storage_Class_EXTERNAL,
                .number_of_aux_symbols = 0,
            };

            if (!length_name_is_in_string_table) OBJECT_MEMCPY(symbol.name.short_name, length_name, length_name_length);
            else symbol.name.string_table.offset = 4;

            object__write(buffer, &written, &symbol, OBJECT_COFF_SYMBOL_SIZE);
        }

        { // data array
            object_COFF_Symbol symbol = {
                .value = sizeof(unsigned long long), // offset in section header data
                .section_number = 1, // one-based index
                .type = {
                    .complex = object_COFF_Symbol_Complex_Type_ARRAY,
                    .base = object_COFF_Symbol_Base_Type_CHAR,
                },
                .storage_class = object_COFF_Symbol_Storage_Class_EXTERNAL,
                .number_of_aux_symbols = 0,
            };

            if (!array_name_is_in_string_table) OBJECT_MEMCPY(symbol.name.short_name, array_name, array_name_length);
            else if (length_name_is_in_string_table) symbol.name.string_table.offset = 4 + length_name_length + 1;
            else symbol.name.string_table.offset = 4;

            object__write(buffer, &written, &symbol, OBJECT_COFF_SYMBOL_SIZE);
        }

        unsigned string_table_begin = (unsigned)written;
        object__write(buffer, &written, &string_table_size, sizeof string_table_size);
        if (length_name_is_in_string_table) {
            object__write(buffer, &written, length_name, length_name_length);
            buffer[written++] = 0;
        }
        if (array_name_is_in_string_table) {
            object__write(buffer, &written, array_name, array_name_length);
            buffer[written++] = 0;
        }
        OBJECT_ASSERT(string_table_size == (unsigned)written - string_table_begin);
    } else if (flags & object_Bundle_MACHO) {
        OBJECT_ASSERT(!(flags & object_Bundle_COFF));

        unsigned long long bytes_needed = 0;

        bytes_needed += sizeof(object_MachO_File_Header);

        unsigned size_of_load_commands = 0;
        size_of_load_commands += sizeof(object_MachO_Load_Command_Segment_64_Bit) + sizeof(object_MachO_Segment_Section_64_Bit);
        size_of_load_commands += sizeof(object_MachO_Load_Command_Symbol_Table);
        size_of_load_commands += sizeof(object_MachO_Load_Command_Symbol_Table_Info);
        size_of_load_commands += sizeof(object_MachO_Load_Command_Min_OS_Version);
        bytes_needed += size_of_load_commands;

        unsigned alignment_power_of_2 = 3;
        unsigned long long align = (unsigned long long)(1 << alignment_power_of_2);
        unsigned long long align_padding = (align - (bytes_needed & (align - 1))) & (align - 1);
        bytes_needed += align_padding;
        unsigned long long raw_data_offset = bytes_needed;

        bytes_needed += data_size;
        unsigned symbol_table_offset = (unsigned)bytes_needed;

        bytes_needed += 2 * sizeof(object_MachO_Symbol_64_Bit);
        unsigned strings_offset = (unsigned)bytes_needed;

        // add leading underscore and trailing nil byte to each symbol name
        unsigned string_table_size = 1 + (1 + length_name_length + 1) + (1 + array_name_length + 1);
        bytes_needed += (unsigned long long)string_table_size;

        OBJECT_ASSERT(bytes_needed <= 0xffffffff && "Mach-O file offsets are 32-bit");

        if (buffer == 0 || *buffer_size < bytes_needed) {
            *buffer_size = bytes_needed;
            return 0;
        }

        object_MachO_File_Header file_header = {
            // TODO(felix): make endianness and architecture configurable via flags
            .magic = object_MachO_File_Header_Magic_LITTLE_ENDIAN_64_BIT,
            .cpu_type = object_MachO_CPU_Type_ARM64,
            .cpu_subtype = object_MachO_CPU_Subtype_ARM_ALL,
            .file_type = object_MachO_File_Type_RELOCATABLE_OBJECT,
            .number_of_load_commands = 4,
            .size_of_load_commands = size_of_load_commands,
            .flags = object_MachO_File_HAS_NO_UNDEFINED_REFERENCES,
        };
        object__write(buffer, &written, &file_header, sizeof file_header);
        unsigned long long file_header_end_offset = written;

        const char *segment_name = 0;
        unsigned long long segment_name_length = 0;
        {
            static const char segment_name_read_write[] = "__DATA";
            static const char segment_name_read_only[] = "__TEXT";
            if (write) {
                segment_name = segment_name_read_write;
                segment_name_length = sizeof(segment_name_read_write) - 1;
            } else if (read) {
                segment_name = segment_name_read_only;
                segment_name_length = sizeof(segment_name_read_only) - 1;
            } else OBJECT_ASSERT(0 && "unreachable");
        }

        object_MachO_Load_Command_Segment_64_Bit segment_command = {
            .type = object_MachO_Load_Command_Type_SEGMENT_64_BIT,
            .command_size = sizeof segment_command + sizeof(object_MachO_Segment_Section_64_Bit),
            .address = 0, // placeholder, linker will assign real value
            .address_size = data_size,
            .file_offset = (unsigned)raw_data_offset,
            .size = data_size,
            .maximum_memory_protections =
                object_MachO_Memory_Permission_READ |
                object_MachO_Memory_Permission_WRITE,
            .initial_virtual_memory_protections =
                (object_MachO_Memory_Permission_READ * (object_MachO_Memory_Permissions)read) |
                (object_MachO_Memory_Permission_WRITE * (object_MachO_Memory_Permissions)write),
            .number_of_sections = 1,
            .flag32 = object_MachO_Segment_Flag_NO_RELOCATION |
                object_MachO_Segment_Flag_READ_ONLY_AFTER_RELOCATIONS * (object_MachO_Segment_Flags)read,
        };
        OBJECT_MEMCPY(segment_command.segment_name, segment_name, segment_name_length);
        object__write(buffer, &written, &segment_command, sizeof segment_command);

        object_MachO_Segment_Section_64_Bit segment_section = {
            .address = 0, // placeholder, linker will assign real value
            .size = segment_command.size,
            .file_offset = (unsigned)segment_command.file_offset,
            .alignment_power_of_2 = alignment_power_of_2,
            .flag_or_type = 0,
        };
        if (write) OBJECT_MEMCPY(segment_section.name, "__data", 6);
        else OBJECT_MEMCPY(segment_section.name, "__const", 7);
        OBJECT_MEMCPY(segment_section.segment_name, segment_name, segment_name_length);
        object__write(buffer, &written, &segment_section, sizeof segment_section);

        object_MachO_Load_Command_Symbol_Table symbol_table_command = {
            .type = object_MachO_Load_Command_Type_SYMBOL_TABLE,
            .command_size = sizeof symbol_table_command,
            .symbols_file_offset = symbol_table_offset,
            .number_of_symbols = 2,
            .string_table_file_offset = strings_offset,
            .string_table_size = string_table_size,
        };
        object__write(buffer, &written, &symbol_table_command, sizeof symbol_table_command);

        object_MachO_Load_Command_Symbol_Table_Info symbol_table_info_command = {
            .type = object_MachO_Load_Command_Type_SYMBOL_TABLE_INFO,
            .command_size = sizeof symbol_table_info_command,
            .external_symbols_index = 0,
            .number_of_external_symbols = 2,
            .undefined_symbols_index = 2,
        };
        object__write(buffer, &written, &symbol_table_info_command, sizeof symbol_table_info_command);

        object_MachO_Load_Command_Min_OS_Version min_os_version_command = {
            .type = object_MachO_Load_Command_Type_MIN_OS_VERSION,
            .command_size = sizeof min_os_version_command,
            .platform_type = object_MachO_Platform_MACOS,
            .min_os_version = object__macho_version(10, 6, 0), // seems reasonable to say 10.6 given this is when Mach-O apparently had breaking changes (see Wikipedia)
            .sdk_version = 0,
            .number_of_tools_used = 0, // haven't found a reason yet for the object file to pretend it was generated by Clang/Swift/LD
        };
        object__write(buffer, &written, &min_os_version_command, sizeof min_os_version_command);

        unsigned long long load_commands_end_offset = written;
        OBJECT_ASSERT(load_commands_end_offset - file_header_end_offset == file_header.size_of_load_commands);

        unsigned long long align_padding_recalculation = (align - (written & (align - 1))) & (align - 1);
        OBJECT_ASSERT(align_padding_recalculation == align_padding);
        object__write(buffer, &written, "\0\0\0\0\0\0\0", align_padding);
        OBJECT_ASSERT(written == raw_data_offset);

        {
            object__write(buffer, &written, &data_size_without_length, sizeof data_size_without_length);
            data_in_object_file = &buffer[written];

            // if data == 0, user will write
            if (data != 0) OBJECT_MEMCPY(buffer + written, data, data_size_without_length);
            written += data_size_without_length;
        }

        OBJECT_ASSERT(written == (unsigned long long)symbol_table_command.symbols_file_offset);

        object_MachO_Symbol_64_Bit length_symbol = {
            .name_offset = 1,
            .type =
                object_MachO_Symbol_Type_DEFINED_IN_SECTION_NUMBER |
                object_MachO_Symbol_Type_EXTERNAL,
            .section_number = 1,
            .data_info = {0},
            .address = 0,
        };
        object__write(buffer, &written, &length_symbol, sizeof length_symbol);

        object_MachO_Symbol_64_Bit array_symbol = {
            .name_offset = 1 + (1 + length_name_length + 1),
            .type =
                object_MachO_Symbol_Type_DEFINED_IN_SECTION_NUMBER |
                object_MachO_Symbol_Type_EXTERNAL,
            .section_number = 1,
            .data_info = {0},
            .address = data_size - data_size_without_length,
        };
        OBJECT_ASSERT(array_symbol.address == sizeof(unsigned long long));
        object__write(buffer, &written, &array_symbol, sizeof array_symbol);

        OBJECT_ASSERT(written == (unsigned long long)symbol_table_command.string_table_file_offset);
        buffer[written++] = 0; // string table has leading nil by convention
        {
            buffer[written++] = '_';
            object__write(buffer, &written, length_name, length_name_length);
            buffer[written++] = 0;
        }
        {
            buffer[written++] = '_';
            object__write(buffer, &written, array_name, array_name_length);
            buffer[written++] = 0;
        }
    } else {
        OBJECT_ASSERT(0 && "unsupported or unset object file type; check `flags`");
    }

    *buffer_size = written;
    return data_in_object_file;
}


#endif // OBJECT_IMPLEMENTATION
