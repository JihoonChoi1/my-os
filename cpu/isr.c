// isr.c
#include "isr.h"
#include "idt.h"
#include "ports.h" // Added to use port I/O functions

// PIC (Programmable Interrupt Controller) Port Numbers
// Master PIC
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
// Slave PIC
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// External declaration for the printing functions (defined in kernel.c)
extern void print_string(char *str);
extern void print_hex(int n);

// ----------------------------------------------------------------
// Function to initialize the PIC chips and remap interrupts to 32-47
// ----------------------------------------------------------------
void pic_remap()
{
  // ICW1: Start initialization command (0x11)
  // "Wait for 4 initialization words to be sent in sequence!"
  port_byte_out(PIC1_COMMAND, 0x11);
  port_byte_out(PIC2_COMMAND, 0x11);

  // ICW2: Set the vector offset (Remapping - Critical!)
  // Master PIC starts at interrupt 32 (0x20)
  port_byte_out(PIC1_DATA, 0x20);
  // Slave PIC starts at interrupt 40 (0x28)
  port_byte_out(PIC2_DATA, 0x28);

  // ICW3: Configure Master/Slave cascading
  // Master: "There is a Slave connected to IRQ line 2" (0x04 = 0000 0100)
  port_byte_out(PIC1_DATA, 0x04);
  // Slave: "I am connected to the Master's IRQ line 2" (0x02)
  port_byte_out(PIC2_DATA, 0x02);

  // ICW4: Set environment (8086 mode)
  port_byte_out(PIC1_DATA, 0x01);
  port_byte_out(PIC2_DATA, 0x01);

  // Set Mask: 
  // Master PIC: Enable IRQ 0 (Timer) & IRQ 1 (Keyboard) -> 1111 1100 = 0xFC
  // Slave PIC: Disable all -> 1111 1111 = 0xFF
  port_byte_out(PIC1_DATA, 0xFC);
  port_byte_out(PIC2_DATA, 0xFF);
}

// Handler for Interrupt 0 (Division By Zero)
void isr0_handler()
{
  print_string("\n[!] EXCEPTION: Division By Zero!\n");
  print_string("System Halted.\n");

  // Halt the system with an infinite loop since a fatal error occurred
  while (1)
    ;
}

// Include Memory Managers
#include "../mm/vmm.h"
#include "../mm/pmm.h"

// Defined in mm/vmm.c (Global)
extern void copy_page_physical(uint32_t src, uint32_t dest);

// Handler for Interrupt 14 (Page Fault)
void page_fault_handler(registers_err_t *regs)
{
    uint32_t faulting_address;
    // Read CR2 register to get the address that caused the fault
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    // Error Code (regs->err_code):
    // Bit 0: Present (0=Not Present, 1=Protection Violation)
    // Bit 1: Write (0=Read, 1=Write)
    // Bit 2: User (0=Kernel, 1=User)
    // Bit 3: Reserved Write (1=Reserved Bit Violation in PTE)
    // Bit 4: Instruction Fetch (1=Fetch, 0=Data Access)
    
    int present = regs->err_code & 0x1;
    int rw = regs->err_code & 0x2;
    int user = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;
    int id = regs->err_code & 0x10;

    // ---------------------------------------------------------
    // COPY-ON-WRITE (COW) HANDLER
    // ---------------------------------------------------------
    // Condition:
    // 1. It is a Write Fault (rw=1)
    // 2. The Page IS Present (present=1) -> So it's a Protection Violation (Read-Only)
    // 3. The PTE has the COW bit set.
    
    if (present && rw) {
        // Walk the Page Table to check COW bit
        uint32_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        
        // CR3 points to Physical Directory. Convert to Virtual to access.
        page_directory* pd = (page_directory*)P2V(cr3);
        
        uint32_t pd_index = faulting_address >> 22;
        uint32_t pt_index = (faulting_address >> 12) & 0x03FF;
        
        if (pd->m_entries[pd_index] & I86_PTE_PRESENT) {
            uint32_t pt_phys = pd->m_entries[pd_index] & I86_PTE_FRAME;
            page_table* pt = (page_table*)P2V(pt_phys);
            
            if (pt->m_entries[pt_index] & I86_PTE_PRESENT) {
                // Check if Custom COW Bit is set
                if (pt->m_entries[pt_index] & I86_PTE_COW) {
                    
                    // --- COW DETECTED: DUPLICATE PAGE ---
                    
                    uint32_t old_frame = pt->m_entries[pt_index] & I86_PTE_FRAME;
                    
                    // 1. Check Reference Count
                    // If RefCount == 1, we are the only owner.
                    // Just Unmark COW and Make Writable (Optimization)
                    if (pmm_get_ref(old_frame) == 1) {
                         pt->m_entries[pt_index] |= I86_PTE_WRITABLE;
                         pt->m_entries[pt_index] &= ~I86_PTE_COW;
                    } else {
                        // Shared Page (Ref > 1) -> Must Alloc New
                        uint32_t new_frame = pmm_alloc_block();
                        if (!new_frame) {
                            print_string("COW Error: Out of Memory\n");
                            while(1);
                        }
                        
                        // 2. Copy Data (Old -> New)
                        copy_page_physical(old_frame, new_frame);
                        
                        // 3. Update PTE to point to New Frame
                        uint32_t flags = pt->m_entries[pt_index] & 0x0FFF;
                        flags |= I86_PTE_WRITABLE; // Restore Write
                        flags &= ~I86_PTE_COW;     // Clear COW
                        
                        pt->m_entries[pt_index] = new_frame | flags;
                        
                        // 4. Decrement Ref Count of Old Frame
                        pmm_free_block(old_frame); // (Functions as dec_ref)
                    }
                    
                    // 5. Invalidate TLB for this address
                    __asm__ volatile("invlpg (%0)" ::"r" (faulting_address) : "memory");
                    
                    return; // Resume Execution!
                }
            }
        }
    }

    // Standard Panic Output
    print_string("\n[!] EXCEPTION: Page Fault!\n");
    print_string("Faulting Address: ");
    print_hex(faulting_address);
    print_string("\n");
    
    print_string("Error Code: ");
    print_hex(regs->err_code);
    print_string(" (");
    if (present) print_string("Protection "); else print_string("NotPresent ");
    if (rw) print_string("Write "); else print_string("Read ");
    if (user) print_string("User "); else print_string("Kernel ");
    print_string(")\n");

    print_string("System Halted.\n");

    while (1) {
        __asm__ volatile("hlt");
    }
}

// Handler for IRQ 1 (Keyboard)
// Moved to keyboard.c
// void keyboard_handler() { ... }