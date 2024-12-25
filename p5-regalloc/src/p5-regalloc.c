/**
 * Name: Gillian Kelly, Annalise Lang
 * 
 * @file p5-regalloc.c
 * @brief Compiler phase 5: register allocation
 */
#include "p5-regalloc.h" 

#define INFINITY MAX_INSTRUCTIONS

#define INVALID -1 // indicates empty register

int offset[MAX_VIRTUAL_REGS]; // stack offset for spilled registers

int allocate(Operand vr, int *name, int size, ILOCInsn* prev_insn, ILOCInsn* stack_allocator, InsnList* list);
int ensure(Operand vr, int *name, int size, ILOCInsn* prev_insn, ILOCInsn* stack_allocator, InsnList* list);
int dist(int vr, ILOCInsn* insn, InsnList* list);
void spill(int pr, int* name, ILOCInsn* prev_insn, ILOCInsn* stack_allocator);
/**
 * @brief Replace a virtual register id with a physical register id
 * 
 * Every virtual register operand with ID "vr" will be replaced by a physical
 * register operand with ID "pr" in the given instruction.
 * 
 * @param vr Virtual register id that should be replaced
 * @param pr Physical register id that it should be replaced by
 * @param isnsn Instruction to modify
 */
void replace_register(int vr, int pr, ILOCInsn* insn)
{
    for (int i = 0; i < 3; i++) {
        if (insn->op[i].type == VIRTUAL_REG && insn->op[i].id == vr) {
            insn->op[i].type = PHYSICAL_REG;
            insn->op[i].id = pr;
        }
    }
}

/**
 * @brief Insert a store instruction to spill a register to the stack
 * 
 * We need to allocate a new slot in the stack from for the current
 * function, so we need a reference to the local allocator instruction.
 * This instruction will always be the third instruction in a function
 * and will be of the form "add SP, -X => SP" where X is the current
 * stack frame size.
 * 
 * @param pr Physical register id that should be spilled
 * @param prev_insn Reference to an instruction; the new instruction will be
 * inserted directly after this one
 * @param local_allocator Reference to the local frame allocator instruction
 * @returns BP-based offset where the register was spilled
 */
int insert_spill(int pr, ILOCInsn* prev_insn, ILOCInsn* local_allocator)
{
    /* adjust stack frame size to add new spill slot */
    int bp_offset = local_allocator->op[1].imm - WORD_SIZE;
    local_allocator->op[1].imm = bp_offset;

    /* create store instruction */
    ILOCInsn* new_insn = ILOCInsn_new_3op(STORE_AI,
            physical_register(pr), base_register(), int_const(bp_offset));

    /* insert into code */
    new_insn->next = prev_insn->next;
    prev_insn->next = new_insn;

    return bp_offset;
}

/**
 * @brief Insert a load instruction to load a spilled register
 * 
 * @param bp_offset BP-based offset where the register value is spilled
 * @param pr Physical register where the value should be loaded
 * @param prev_insn Reference to an instruction; the new instruction will be
 * inserted directly after this one
 */
void insert_load(int bp_offset, int pr, ILOCInsn* prev_insn)
{
    /* create load instruction */
    ILOCInsn* new_insn = ILOCInsn_new_3op(LOAD_AI,
            base_register(), int_const(bp_offset), physical_register(pr));

    /* insert into code */
    new_insn->next = prev_insn->next;
    prev_insn->next = new_insn;
}

void allocate_registers (InsnList* list, int num_physical_registers)
{
    // terminate if list is null
    if (list == NULL || num_physical_registers < 1) 
    {
        exit(0);
    }

    // reinitialize local data structs 
    int size = num_physical_registers;
    int name[size];     // mapping of phys_reg => vr

    ILOCInsn* stack_allocator = NULL;
    ILOCInsn* prev_ins = NULL;

    for (int i = 0; i < size; i++) {
        name[i] = INVALID;
    }

    // for each instruction i in program:
    FOR_EACH(ILOCInsn*, i, list) {
        // *save reference to stack allocator instruction if i is a call label
        if (i->form == LABEL && i->op[0].type == CALL_LABEL) {
            stack_allocator = i->next->next->next;
        }

        // for each read vr in i:
        ILOCInsn* read_regs = ILOCInsn_get_read_registers(i);
        for (int op = 0; op < 3; op++) {
            Operand vr = read_regs->op[op];
            if (vr.type == VIRTUAL_REG) {
                // make sure vr is in a phys reg
                int pr = ensure(vr, name, size, prev_ins, stack_allocator, list);
                // change register id
                replace_register(vr.id, pr, i);
                /* this part allows reuse of registers that are no longer needed */
                if (dist(vr.id, i, list) == INFINITY) {
                    name[pr] = INVALID;
                } 
            }
        }
        ILOCInsn_free(read_regs);

        // for each written vr in i:
        Operand write_reg = ILOCInsn_get_write_register(i);
        if (write_reg.type == VIRTUAL_REG) {
            // make sure phys_reg is available
            int pr = allocate(write_reg, name, size, prev_ins, stack_allocator, list);
            replace_register(write_reg.id, pr, i);
        }

        // *spill any live registers before procedure calls
        if (i->form == CALL) {
            for (int pr = 0; pr < size; pr++) {
                if (name[pr] != INVALID)
                    spill(pr, name, prev_ins, stack_allocator);
            }
        }

        // *save reference to i to facilitate spilling before next instruction
        prev_ins = i;
        

    }   
    
}

/*
 * returns a physical register from the free list, if one exists. Otherwise,
 * it selects the value stored in name that is farthest in the future, spills it,
 * and reallocates the corresponding physical register.
 * 
 * @param vr -- virtual register needing a physical register
 * @param name -- array mapping phys_reg to vr
 * @size -- num of physical registers
 */
int allocate(Operand vr, int *name, int size, ILOCInsn* prev_insn, ILOCInsn* stack_allocator, InsnList* list) {
    ILOCInsn* curr = prev_insn->next;
    // if there's a free register, allocate and use it
    for (int pr = 0; pr < size; pr++) {
        if (name[pr] == INVALID) {
            name[pr] = vr.id;
            return pr;
        }
    }
    // find pr that maximizes dist(name[pr])   // otherwise, find register to spill
    int max_dist = 0;
    int best_pr = 0;
    for (int pr = 0; pr < size; pr++) {
        int distance = dist(name[pr], curr, list);
        if (distance > max_dist) {
            max_dist = distance;
            best_pr = pr;
        }  
    }

    // spill value to stack
    spill(best_pr, name, prev_insn, stack_allocator);
    // reallocate it
    name[best_pr] = vr.id;
    // and use it
    return best_pr;
}

/*
 * return physical register for virtual register.
 */
int ensure(Operand vr, int *name, int size, ILOCInsn* prev_insn, ILOCInsn* stack_allocator, InsnList* list) {    
    for (int pr = 0; pr < size; pr++) {
        if (name[pr] == vr.id) {
            return pr;
        }
    }
    int pr = allocate(vr, name, size, prev_insn, stack_allocator, list);
    // if vr was spilled, load it 
    if (offset[vr.id] != INVALID) {
        // emit load into pr from offset[vr]
        insert_load(offset[vr.id], pr, prev_insn);
    }
    return pr;     
 }

/*
 * calcualte the distance from the instruction that is currently being allocated 
 */
int dist(int vr, ILOCInsn* insn, InsnList* list) {
    // return number of instructions until vr is next used (INFINITY if no use)
    // i.e. return the idx in the block of the next reference to vr 
    int distance = 0;
    ILOCInsn* next_ins = insn->next;
    while (next_ins != NULL) {
        for (int i = 0; i < 3; i++) {
            if (next_ins->op[i].type == VIRTUAL_REG && next_ins->op[i].id == vr) {
                return distance;
            }
        }
        next_ins = next_ins->next;
        distance++;
    }

    return INFINITY;
}

void spill(int pr, int* name, ILOCInsn* prev_insn, ILOCInsn* stack_allocator) {
    int bp_offset = insert_spill(pr, prev_insn, stack_allocator);
    offset[name[pr]] = bp_offset;
    name[pr] = INVALID;
 }

