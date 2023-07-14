AI5WIN Memory Model
-------------------

### 16-bit address space

AI5WIN uses a 16-bit address space layed out according to the following struct:

```c
struct mem16 {
    char mes_name[128];
    int8_t var4[NR_VAR4];
    int16_t *system_var16_ptr;
    int16_t var16[26];
    int16_t system_var16[26];
    int32_t var32[26];
    int32_t system_var32[26];
    int8_t heap[HEAP_SIZE];
};
```

In YU-NO (elf Classics version), it is initialized as follows:

```c
void init_mem16(struct mem16 *mem)
{
    memset(mem, 0, sizeof(struct mem16));

    mem->system_var16_ptr = mem->system_var16;

    // system variables
    mem->system_var16[2] = 0x260d; // flags
    mem->system_var16[5] = 0;      // text home x
    mem->system_var16[6] = 0;      // text home y
    mem->system_var16[7] = 640;    // width (?)
    mem->system_var16[8] = 400;    // height (?)
    mem->system_var16[12] = 16;    // font width
    mem->system_var16[13] = 16;    // font height
    mem->system_var16[15] = 16;    // font width 2 (?)
    mem->system_var16[16] = 16;    // font height 2 (?)
    mem->system_var16[23] = 0;     // mask color (?)

    // various pointers to VM structures in 32-bit address space
    mem->system_var32[0] = mem;
    mem->system_var32[5] = g_palette_colors;
    mem->system_var32[7] = g_file_data;
    mem->system_var32[8] = g_menu_entry_addresses;
    mem->system_var32[9] = g_menu_entry_numbers;

    mem->system_var32[1] = 0x20000; // ?
}
```

`mes_name` contains the currently executing .mes file name.

`var4` is an array of 4-bit variables (the top 4 bits of each byte are masked
off and unused). The size of this array varies by game. I have observed sizes
of 2048 and 4096.

`system_var16_ptr` is (initially) a pointer to `system_var16`.

`var16` is an array of 16-bit variables. When a value in this array is used as
a pointer, it is an offset into the 16-bit address space. These variables are
named according to the letters of the alphabet, from A to Z.

`system_var16` is an array of 16-bit variables which are used by the VM. They
can be modified to alter the VM's behavior.

`var32` is an array of 32-bit variables. When a value in this array is used as
a pointer, it is a pointer into the VM's own address space. These variables
are named according to the letters of the alphabet, from dwA to dwZ.

`system_var32` is an array of 32-bit variables which are used by the VM. Many
are pointers to VM structures, such as the palette colors, or to the start of
the 16-bit address space.

Expression opcodes 0xa0 and 0xc0 use a 16-bit pointer from the `mem16->var16`
array to read memory from the 16-bit address space.

SETAC and SETA@ statements use a 16-bit pointer from the `mem16->var16` array
to write memory in the 16-bit address space.

### 32-bit address space

The 32-bit address space is the VM's own address space. The VM initializes
`mem16->system_var32` to contain pointers of interest which the game can read
or write.

Expression opcodes 0xf5 and 0xf6 use a 32-bit pointer from the `mem16->var32`
array to read memory from the 32-bit address space.

SETAB, SETAW and SETAD statements use a 32-bit pointer from the `mem16->var32`
array to write memory in the 32-bit address space.

A 64-bit implementation would have to create a virtual 32-bit address space
similar to how the 16-bit address space is implemented (described above).
