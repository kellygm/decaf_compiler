/**
 * @file p4-codegen.c
 * @brief Compiler phase 4: code generation
 * 
 * @author Gillian Kelly, Annalise Lang
 */

#include "p4-codegen.h"

typedef struct LoopContext {
    Operand start_label;
    Operand continue_label; // label to 'continue' to
    Operand end_label;
    struct LoopContext* next; // for nesting conditionals and while loops
} LoopContext;

/**
 * @brief State/data for the code generator visitor
 */
typedef struct CodeGenData
{
    /**
     * @brief Reference to the epilogue jump label for the current function
     */
    Operand current_epilogue_jump_label;

    /* add any new desired state information (and clean it up in CodeGenData_free) */
    Operand previous_epilogue_jump_label;
    Operand current_jump_label;
    Operand loop_label;
    Operand bp;
    Operand sp;
    int local_size;     // local variables
    int stack_size;     // global variables
    bool in_function;   
    LoopContext* loop_context;

} CodeGenData;

/**
 * @brief Allocate memory for code gen data
 * 
 * @returns Pointer to allocated structure
 */
CodeGenData* CodeGenData_new (void)
{
    CodeGenData* data = (CodeGenData*)calloc(1, sizeof(CodeGenData));
    CHECK_MALLOC_PTR(data);
    data->current_epilogue_jump_label = empty_operand ();
    data->previous_epilogue_jump_label = data->current_epilogue_jump_label;
    data->current_jump_label = empty_operand ();
    return data;
}

/**
 * @brief Deallocate memory for code gen data
 * 
 * @param data Pointer to the structure to be deallocated
 */
void CodeGenData_free (CodeGenData* data)
{
    /* free everything in data that is allocated on the heap */

    /* free "data" itself */
    free(data);
}

/**
 * @brief Macro for more convenient access to the error list inside a @c visitor
 * data structure
 */
#define DATA ((CodeGenData*)visitor->data)

/**
 * @brief Fills a register with the base address of a variable.
 * 
 * @param node AST node to emit code into (if needed)
 * @param variable Desired variable
 * @returns Virtual register that contains the base address
 */
Operand var_base (ASTNode* node, Symbol* variable)
{
    Operand reg = empty_operand();
    switch (variable->location) {
        case STATIC_VAR:
            reg = virtual_register();
            ASTNode_emit_insn(node,
                    ILOCInsn_new_2op(LOAD_I, int_const(variable->offset), reg));
            break;
        case STACK_PARAM:
        case STACK_LOCAL:
            reg = base_register();
            break;
        default:
            break;
    }
    return reg;
}

/**
 * @brief Calculates the offset of a scalar variable reference and fills a register with that offset.
 * 
 * @param node AST node to emit code into (if needed)
 * @param variable Desired variable
 * @returns Virtual register that contains the base address
 */
Operand var_offset (ASTNode* node, Symbol* variable)
{
    Operand op = empty_operand();
    switch (variable->location) {
        case STATIC_VAR:    op = int_const (0); break;
        case STACK_PARAM:
        case STACK_LOCAL:   op = int_const (variable->offset); break;
        default:            break;
    }
    return op;
}

#ifndef SKIP_IN_DOXYGEN

/*
 * Macros for more convenient instruction generation
 */

#define EMIT0OP(FORM)             ASTNode_emit_insn(node, ILOCInsn_new_0op(FORM))
#define EMIT1OP(FORM,OP1)         ASTNode_emit_insn(node, ILOCInsn_new_1op(FORM,OP1))
#define EMIT2OP(FORM,OP1,OP2)     ASTNode_emit_insn(node, ILOCInsn_new_2op(FORM,OP1,OP2))
#define EMIT3OP(FORM,OP1,OP2,OP3) ASTNode_emit_insn(node, ILOCInsn_new_3op(FORM,OP1,OP2,OP3))

void CodeGenVisitor_gen_program (NodeVisitor* visitor, ASTNode* node)
{
    /*
     * make sure "code" attribute exists at the program level even if there are
     * no functions (although this shouldn't happen if static analysis is run
     * first); also, don't include a print function here because there's not
     * really any need to re-print all the functions in the program node *
     */
    ASTNode_set_attribute(node, "code", InsnList_new(), (Destructor)InsnList_free);

    /* copy code from each function */
    FOR_EACH(ASTNode*, func, node->program.functions) {
        ASTNode_copy_code(node, func);
    }

    // alloc space for global variables "staticSize"


}

void CodeGenVisitor_previsit_funcdecl (NodeVisitor* visitor, ASTNode* node)
{
    /* generate a label reference for the epilogue that can be used while
     * generating the rest of the function (e.g., to be used when generating
     * code for a "return" statement) */
    DATA->current_epilogue_jump_label = anonymous_label();
    DATA->in_function = true;
    DATA->sp = stack_register ();
    DATA->bp = base_register ();
}

void CodeGenVisitor_gen_funcdecl (NodeVisitor* visitor, ASTNode* node)
{
    /* every function begins with the corresponding call label */
    EMIT1OP (LABEL, call_label (node->funcdecl.name));

    /* PROLOGUE */
    // Save caller's base pointer by pushing it on the stack
    EMIT1OP (PUSH, DATA->bp);

    // set the callee's base pointer using the stack pointer
    EMIT2OP (I2I, DATA->sp, DATA->bp);

    // alloc space for local variables and spilled registers by decrementing the stack pointer
    EMIT3OP (ADD_I, DATA->sp, int_const (DATA->local_size * LOCAL_BP_OFFSET), DATA->sp);

    /* copy code from body */
    ASTNode_copy_code (node, node->funcdecl.body);

    EMIT1OP (LABEL, DATA->current_epilogue_jump_label);

    /* EPILOGUE */
    // save return value (done in gen_return; may need to move??)
    // restore stack pointer from base pointer
    EMIT2OP (I2I, DATA->bp, DATA->sp);
    // restore caller's base pointer (pop) and jump
    EMIT1OP (POP, DATA->bp);
    EMIT0OP (RETURN);
    DATA->in_function = false;
    DATA->local_size = 0;
}

void CodeGenVisitor_gen_block (NodeVisitor *visitor, ASTNode *node)
{
    /* copy code from each statement */
    int stmt_count = 0;
    FOR_EACH(ASTNode*, stmt, node->block.statements) {
        ASTNode_copy_code (node, stmt);
        if (stmt->type == ASSIGNMENT) {
            stmt_count++;
        }
    }

    // update local variables count
    if (DATA->in_function) {
        DATA->local_size = stmt_count;
    }
}

void CodeGenVisitor_gen_return (NodeVisitor *visitor, ASTNode *node)
{
    // get the return value and copy it to the return node
    ASTNode_copy_code (node, node->funcreturn.value);
    // move return value to the return register
    EMIT2OP (I2I, ASTNode_get_temp_reg (node->funcreturn.value), return_register ());
    // jump to the epilogue to handle function exit
    EMIT1OP (JUMP, DATA->current_epilogue_jump_label);
}

void CodeGenVisitor_gen_cond (NodeVisitor *visitor, ASTNode *node) {
    // copy conditional and evalute 
    ASTNode_copy_code (node, node->conditional.condition);
    Operand cond_reg = ASTNode_get_temp_reg (node->conditional.condition);

    // label for control blocks
    Operand if_label = anonymous_label ();
    Operand else_label = node->conditional.else_block ? anonymous_label () : empty_operand ();
    Operand post_label = anonymous_label ();

    // emit conditional branch
    if (node->conditional.else_block) {
        EMIT3OP (CBR, cond_reg, if_label, else_label);
    } else {
        EMIT3OP (CBR, cond_reg, if_label, post_label);
    }

    // emit if-block
    EMIT1OP (LABEL, if_label);
    DATA->current_jump_label = if_label;
    ASTNode_copy_code (node, node->conditional.if_block);
    if (node->conditional.else_block) {
        EMIT1OP (JUMP, post_label);
    }

    // emit else-block
    if (node->conditional.else_block) {
        EMIT1OP (LABEL, else_label);
        DATA->current_jump_label = else_label;
        ASTNode_copy_code (node, node->conditional.else_block);
    }

    // emit post-block (code after if-else)
    EMIT1OP (LABEL, post_label);
    DATA->current_jump_label = post_label;
}

void CodeGenVisitor_gen_literal (NodeVisitor *visitor, ASTNode *node)
{
    // load literal type into a temporary immediate register based on its type
    switch (node->literal.type) {
        case INT:
            ASTNode_set_temp_reg (node, virtual_register ());
            EMIT2OP (LOAD_I, int_const (node->literal.integer), ASTNode_get_temp_reg (node));
            break;
        case STR:
            ASTNode_set_temp_reg (node, virtual_register ());
            EMIT2OP (LOAD_I, str_const (node->literal.string), ASTNode_get_temp_reg (node));
            break;
        case BOOL:
            ASTNode_set_temp_reg (node, virtual_register ());
            EMIT2OP (LOAD_I, int_const (node->literal.boolean), ASTNode_get_temp_reg (node));
            break;
        default:
            break;
    }
}

void CodeGenVisitor_gen_binop (NodeVisitor *visitor, ASTNode *node) {
    // copy code from left & right child
    ASTNode_copy_code (node, node->binaryop.left);
    ASTNode_copy_code (node, node->binaryop.right);

    // alloc new virtual register for result of binop
    ASTNode_set_temp_reg (node, virtual_register ());

    // emit appropriate instruction based on operator
    switch(node->binaryop.operator) {
        case ADDOP:
            // check for ADD_I
            if (node->binaryop.left->type == LITERAL) {
                Operand immd = int_const (node->binaryop.right->literal.integer);
                EMIT3OP (ADD_I, ASTNode_get_temp_reg (node->binaryop.left), immd, ASTNode_get_temp_reg (node));
            }
            EMIT3OP (ADD, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node));            break;
        case SUBOP:
            EMIT3OP (SUB, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node));            break;
        case MULOP:
            // check for MULT_I
            if (node->binaryop.left->type == LITERAL) {
                Operand immd = int_const(node->binaryop.right->literal.integer);
                EMIT3OP (MULT_I, ASTNode_get_temp_reg (node->binaryop.left), immd, ASTNode_get_temp_reg (node));
            }
            EMIT3OP (MULT, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node));           break;
        case DIVOP: EMIT3OP (DIV, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node));    break;
        case MODOP:
            Operand op1 = virtual_register ();
            Operand op2 = virtual_register ();
            // divide L and R sides of binary op
            EMIT3OP (DIV, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), op1);
            // multiply the division by the right side 
            EMIT3OP (MULT, ASTNode_get_temp_reg (node->binaryop.right), op1, op2);
            // subtract the left side by the multiplication
            EMIT3OP (SUB, ASTNode_get_temp_reg (node->binaryop.left), op2, ASTNode_get_temp_reg (node));
            break;
        case OROP:  EMIT3OP (OR,     ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case ANDOP: EMIT3OP (AND,    ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case EQOP:  EMIT3OP (CMP_EQ, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case NEQOP: EMIT3OP (CMP_NE, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case LTOP:  EMIT3OP (CMP_LT, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case LEOP:  EMIT3OP (CMP_LE, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case GEOP:  EMIT3OP (CMP_GE, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        case GTOP:  EMIT3OP (CMP_GT, ASTNode_get_temp_reg (node->binaryop.left), ASTNode_get_temp_reg (node->binaryop.right), ASTNode_get_temp_reg (node)); break;
        default:    break;
    }
}

void CodeGenVisitor_gen_unop (NodeVisitor *visitor, ASTNode *node) {
    // copy code from child node
    ASTNode_copy_code (node, node->unaryop.child);
    // alloc temp register for result 
    ASTNode_set_temp_reg (node, virtual_register ());

    // emit inst based on operator type
    switch(node->unaryop.operator) {
        case NEGOP: EMIT2OP (NEG, ASTNode_get_temp_reg (node->unaryop.child), ASTNode_get_temp_reg (node)); break;
        case NOTOP: EMIT2OP (NOT, ASTNode_get_temp_reg (node->unaryop.child), ASTNode_get_temp_reg (node)); break;
        default: break;
    }
}

void CodeGenVisitor_gen_location(NodeVisitor *visitor, ASTNode *node) {
    Symbol *var_sym = lookup_symbol (node, node->location.name);
    Operand base_reg = var_base (node, var_sym);
    Operand value_reg = virtual_register ();
    Operand offset_reg;

    if (var_sym->symbol_type == ARRAY_SYMBOL) {
        // evaluted index FIRST
        ASTNode_copy_code (node, node->location.index);
        Operand idx_reg = ASTNode_get_temp_reg (node->location.index);

        // calculate offset 
        offset_reg = virtual_register ();
        EMIT3OP (MULT_I, idx_reg, int_const (WORD_SIZE), offset_reg);

        // load value from calculated address
        EMIT3OP (LOAD_AO, base_reg, offset_reg, value_reg);
    } else {
        // load the variable directly
        offset_reg = var_offset (node, var_sym);
        EMIT3OP (LOAD_AI, base_reg, offset_reg, value_reg);
    }

    ASTNode_set_temp_reg (node, value_reg);
}

void CodeGenVisitor_gen_assgn (NodeVisitor *visitor, ASTNode *node) {
    Symbol *var_sym = lookup_symbol (node, node->assignment.location->location.name);
    Operand base_reg = var_base (node, var_sym);
    Operand value_reg, idx_reg, offset_reg;

    ASTNode_copy_code (node, node->assignment.value);
    value_reg = ASTNode_get_temp_reg (node->assignment.value);

    // handle array assignments
    if (var_sym->symbol_type == ARRAY_SYMBOL) {
        ASTNode_copy_code (node, node->assignment.location->location.index);
        idx_reg = ASTNode_get_temp_reg (node->assignment.location->location.index);

        // calculate offset
        offset_reg = virtual_register ();
        EMIT3OP (MULT_I, idx_reg, int_const (WORD_SIZE), offset_reg);

        // store value to calculate address
        EMIT3OP (STORE_AO, value_reg, base_reg, offset_reg);
    } else {
        // store value of assignment at location's register with offset
        offset_reg = var_offset (node, var_sym);
        EMIT3OP (STORE_AI, value_reg, base_reg, offset_reg);
    }
}

void CodeGenVisitor_previsit_while(NodeVisitor *visitor, ASTNode* node) {
    LoopContext *loop_context = malloc (sizeof (LoopContext));
    loop_context->start_label = anonymous_label ();
    loop_context->continue_label = anonymous_label ();
    loop_context->end_label = anonymous_label ();

    // push new context onto the loop context stack
    loop_context->next = DATA->loop_context;
    DATA->loop_context = loop_context;

    // set the label for the beginning of the while loop (guard block)
    DATA->previous_epilogue_jump_label = DATA->current_epilogue_jump_label;
    DATA->loop_label = loop_context->start_label;
}

void CodeGenVisitor_gen_while (NodeVisitor *visitor, ASTNode *node) {
    LoopContext* loop_context = DATA->loop_context;

    // Start by labeling the start of the loop, which is right before the condition check
    EMIT1OP (LABEL, loop_context->start_label);
    DATA->current_jump_label = loop_context->start_label;

    // start at guard expression and evalute its code 
    ASTNode_copy_code (node, node->whileloop.condition);
    Operand cond_reg = ASTNode_get_temp_reg (node->whileloop.condition);
     
    // cemit cbr branch: if cond true, continue to loop body, else jump to end
    EMIT3OP (CBR, cond_reg, loop_context->continue_label, loop_context->end_label);

    // label for continue
    EMIT1OP (LABEL, loop_context->continue_label);
    DATA->current_jump_label = loop_context->continue_label;

    // generate code for loop body
    ASTNode_copy_code (node, node->whileloop.body);

    // jump back to start to reevaluate guard condition
    EMIT1OP (JUMP, DATA->loop_context->start_label);

    // label for end of loop, code that comes after it 
    EMIT1OP (LABEL, DATA->loop_context->end_label);
    DATA->current_jump_label = loop_context->end_label;

    // pop current loop off the stack as we leave the loop
    DATA->loop_context = loop_context->next;

    // set current epilogue jump label back to previous
    DATA->current_epilogue_jump_label = DATA->previous_epilogue_jump_label;
    free (loop_context);
}

void CodeGenVisitor_gen_break (NodeVisitor *visitor, ASTNode *node)
{
    EMIT1OP (JUMP, DATA->loop_context->end_label); // jump to the block after the while loop
}

void CodeGenVisitor_gen_continue (NodeVisitor *visitor, ASTNode *node)
{
    EMIT1OP (JUMP, DATA->loop_label); // jump back to the guard block of the while loop
}

void CodeGenVisitor_gen_funccall(NodeVisitor *visitor, ASTNode *node) {
    // handle default print statements
    Symbol *func = lookup_symbol (node, node->funccall.name);
    if (strncmp(func->name, "print_int", 15) == 0 || strncmp(func->name, "print_bool", 15) == 0) {
        // get int and bool
        ASTNode *arg = node->funccall.arguments->head;
        if (arg) {
            ASTNode_copy_code (node, arg);
            EMIT1OP (PRINT, ASTNode_get_temp_reg (arg));
        }
        return;
    } else if (strncmp (func->name, "print_str", 15) == 0) {
        EMIT1OP (PRINT, str_const (node->funccall.arguments->head->literal.string));
        return;
    }

    /* PRECALL */
    // calculate values of all actual parameters IN ORDER
    Operand arg_list [node->funccall.arguments->size];
    int idx = 0;

    FOR_EACH(ASTNode*, arg, node->funccall.arguments) {
        ASTNode_copy_code (node, arg);
        Operand arg_reg = ASTNode_get_temp_reg(arg);
        arg_list[idx++] = arg_reg;
    }

    // push all actual parameter values on the stack IN REVERSE ORDER
    for (int i = node->funccall.arguments->size - 1; i >= 0; i--) {
        EMIT1OP (PUSH, arg_list[i]);
    }

    // call the function
    EMIT1OP (CALL, call_label (node->funccall.name));

    /* POSTRETURN */
    // caller must de-alloc any space allocated for params that were pushed onto stack
    if (node->funccall.arguments->size > 0) {
        EMIT3OP (ADD_I, DATA->sp, int_const (node->funccall.arguments->size * sizeof (int64_t)), DATA->sp);
    }

    // after the call, result is in RET reg. Move it to a new temp reg
    Operand result_reg = virtual_register ();
    EMIT2OP (I2I, return_register (), result_reg);
    ASTNode_set_temp_reg (node, result_reg);
}


#endif
InsnList* generate_code (ASTNode* tree)
{
    InsnList *iloc = InsnList_new ();

    if (!tree) {
        return iloc;
    }

    NodeVisitor *v = NodeVisitor_new ();
    v->data = CodeGenData_new ();
    v->dtor = (Destructor)CodeGenData_free;
    v->postvisit_program     = CodeGenVisitor_gen_program;
    v->previsit_funcdecl     = CodeGenVisitor_previsit_funcdecl;
    v->postvisit_funcdecl    = CodeGenVisitor_gen_funcdecl;
    v->postvisit_block       = CodeGenVisitor_gen_block;
    v->postvisit_return      = CodeGenVisitor_gen_return;
    v->postvisit_literal     = CodeGenVisitor_gen_literal;
    v->postvisit_binaryop    = CodeGenVisitor_gen_binop;
    v->postvisit_unaryop     = CodeGenVisitor_gen_unop;
    v->postvisit_location    = CodeGenVisitor_gen_location;
    v->postvisit_assignment  = CodeGenVisitor_gen_assgn;
    v->postvisit_conditional = CodeGenVisitor_gen_cond;
    v->previsit_whileloop    = CodeGenVisitor_previsit_while;
    v->postvisit_whileloop   = CodeGenVisitor_gen_while;
    v->postvisit_break       = CodeGenVisitor_gen_break;
    v->postvisit_continue    = CodeGenVisitor_gen_continue;
    v->postvisit_funccall    = CodeGenVisitor_gen_funccall;

    /* generate code into AST attributes */
    NodeVisitor_traverse_and_free (v, tree);

    /* copy generated code into new list (the AST may be deallocated before
     * the ILOC code is needed) */
    FOR_EACH (ILOCInsn*, i, (InsnList*) ASTNode_get_attribute(tree, "code")) {
        InsnList_add (iloc, ILOCInsn_copy (i));
    }
    return iloc;
}
