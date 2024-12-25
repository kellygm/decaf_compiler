/**
 * @file p3-analysis.c
 * @brief Compiler phase 3: static analysis
 */
#include "p3-analysis.h"

// previsit functions -- for semantic checks
void AnalysisVisitor_check_program(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_location(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_funcdecl(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_vardecl(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_block(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_assignment(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_conditional(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_while(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_binaryop(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_unaryop(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_funccall(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_literal(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_break_continue(NodeVisitor* visitor, ASTNode* node);
void AnalysisVisitor_check_return(NodeVisitor* visitor, ASTNode* node);


// postvisit functions -- for type inferences 
void Analysis_infer_binaryop(NodeVisitor* visitor, ASTNode* node);
void Analysis_infer_block(NodeVisitor* visitor, ASTNode* node);
void Anaylsis_infer_funccall(NodeVisitor* visitor, ASTNode* node);




/**
 * @brief State/data for static analysis visitor
 */
typedef struct AnalysisData
{
    /**
     * @brief List of errors detected
     */
    ErrorList* errors;

    /* BOILERPLATE: TODO: add any new desired state information (and clean it up in AnalysisData_free) */

} AnalysisData;

/**
 * @brief Allocate memory for analysis data
 * 
 * @returns Pointer to allocated structure
 */
AnalysisData* AnalysisData_new (void)
{
    AnalysisData* data = (AnalysisData*)calloc(1, sizeof(AnalysisData));
    CHECK_MALLOC_PTR(data);
    data->errors = ErrorList_new();
    return data;
}

/**
 * @brief Deallocate memory for analysis data
 * 
 * @param data Pointer to the structure to be deallocated
 */
void AnalysisData_free (AnalysisData* data)
{
    /* free everything in data that is allocated on the heap except the error
     * list; it needs to be returned after the analysis is complete */

    /* free "data" itself */
    free(data);
}

/**
 * @brief Macro for more convenient access to the data inside a @ref AnalysisVisitor
 * data structure
 */
#define DATA ((AnalysisData*)visitor->data)

/**
 * @brief Macro for more convenient access to the error list inside a
 * @ref AnalysisVisitor data structure
 */
#define ERROR_LIST (((AnalysisData*)visitor->data)->errors)

/**
 * @brief Wrapper for @ref lookup_symbol that reports an error if the symbol isn't found
 * 
 * @param visitor Visitor with the error list for reporting
 * @param node AST node to begin the search at
 * @param name Name of symbol to find
 * @returns The @ref Symbol if found, otherwise @c NULL
 */
Symbol* lookup_symbol_with_reporting(NodeVisitor* visitor, ASTNode* node, const char* name)
{
    Symbol* symbol = lookup_symbol(node, name);
    if (symbol == NULL) {
        ErrorList_printf(ERROR_LIST, "Symbol '%s' undefined on line %d", name, node->source_line);
    }
    return symbol;
}

/**
 * @brief Macro for shorter storing of the inferred @c type attribute
 */
#define SET_INFERRED_TYPE(T) ASTNode_set_printable_attribute(node, "type", (void*)(T), \
                                 type_attr_print, dummy_free)

/**
 * @brief Macro for shorter retrieval of the inferred @c type attribute
 */
#define GET_INFERRED_TYPE(N) (DecafType)(long)ASTNode_get_attribute(N, "type")

/**
 * Visitor method for Return nodes to check that the value type matches their function's return type.
 */
void AnalysisVisitor_check_return (NodeVisitor *visitor, ASTNode *node)
{
    ASTNode *returnValue = node->funcreturn.value;
    ASTNode *parentNode = node;

    // get the function node that the return statement is inside
    while (parentNode->type != FUNCDECL) {
        parentNode = (ASTNode *)ASTNode_get_attribute (parentNode, "parent");
    }
    DecafType expectedType = lookup_symbol (parentNode, parentNode->funcdecl.name)->type;
    DecafType receivedType = UNKNOWN;

    if (returnValue != NULL) {
        if (returnValue->type == LITERAL) {
            AnalysisVisitor_check_literal(visitor, returnValue);
            receivedType = GET_INFERRED_TYPE(returnValue);
        } else if (returnValue->type == LOCATION) {
            AnalysisVisitor_check_location(visitor, returnValue);
            receivedType = GET_INFERRED_TYPE(returnValue);
        } 
    } else {
        returnValue = UNKNOWN;
    }

    // TODO: need to add variables (location), binaryops, and unaryops

    if (expectedType != receivedType) {
            ErrorList_printf (ERROR_LIST, "Type mismatch: %s expected but %s found on line %d",
                DecafType_to_string (expectedType), DecafType_to_string (receivedType),
                node->source_line);
    }
}

/**
 * @brief visitor method for literals to check that the value is a valid
 * literal. No infer needed for literals
 */
void AnalysisVisitor_check_literal(NodeVisitor* visitor, ASTNode* node)
{
   // set the inferred literal for later checks 
   SET_INFERRED_TYPE(node->literal.type);
}

/**
 * @brief validate locations and that they are correctly referencing variables
 */
void AnalysisVisitor_check_location(NodeVisitor* visitor, ASTNode* node)
{
    // look up symbol for this location 
    Symbol *symbol = lookup_symbol_with_reporting(visitor, node, node->location.name);
    
    if (symbol == NULL)
    {
        return;
    }

    // infer and set type for this location (scalar or array)
    SET_INFERRED_TYPE(symbol->type);

    // check if its an array
    if (node->location.index != NULL)
    {
        // make sure that the index has been inferred
        if (node->location.index->type == LITERAL)
        {
            AnalysisVisitor_check_literal(visitor, node->location.index);
        }

        // make sure index is an INT and within bounds of array
        if (GET_INFERRED_TYPE(node->location.index) != INT)
        {
            ErrorList_printf(ERROR_LIST, "Array index must be an INT on line %d", node->source_line);
        }
    }
    
    // check the ID is already declared
    // if above is true, make sure inferred type matches literal type
}



void AnalysisVisitor_check_assignment(NodeVisitor* visitor, ASTNode* node)
{
    // get the LOCATION and VALUE of the assignment
    ASTNode* locationNode = node->assignment.location;
    ASTNode* valueNode = node->assignment.value;

    // validate location node using its inferred type
    DecafType locType = UNKNOWN; // initialize to UNKNOWN
    if (locationNode->type == LOCATION)
    {
        // look up symbol in the symbol table
        Symbol *symbol = lookup_symbol_with_reporting(visitor, locationNode, locationNode->location.name);
        if (symbol != NULL)
        {
            locType = symbol->type;
            SET_INFERRED_TYPE(symbol->type);
        }
    }
    else 
    {
        ErrorList_printf(ERROR_LIST, "Invalid assignment on line %d", node->source_line);
        return;
    }

    // validate the  right hand side of the assignment
    // handle checks for literals, locations, funccall, binop, unop
    if (valueNode->type == LITERAL) {
        AnalysisVisitor_check_literal(visitor, valueNode);
    } else if (valueNode->type == LOCATION) {
        AnalysisVisitor_check_location(visitor, valueNode);
    } else if (valueNode->type == BINARYOP) {
        // NOT FINISHED
        return;
    } else if (valueNode->type == UNARYOP){
        // NOT FINISHED
        return;
    } else if (valueNode->type == FUNCCALL) {
        // NOT FINISHED
        return;
    } else {
        return;
    }

    DecafType valueType = GET_INFERRED_TYPE(valueNode);
    if (locType != valueType)
    {
        ErrorList_printf(ERROR_LIST, "Type mismatch: %s is incompatiable with %s on line %d", DecafType_to_string(locType), DecafType_to_string(valueType), node->source_line);
    }
    
    
    // check that the value wishing to be assigned matches the type of the ID
    // check if ID is an index of an array variable 
}


/**
 * @brief ensure semantically correct function declarations
 */
void AnaylsisVisitor_check_funcdecl(NodeVisitor* visitor, ASTNode* node)
{
    // validate type of func
    
    // validate func's parameters
    // check its blocks
    lookup_symbol_with_reporting(visitor, node, "main");
}

/**
 * Visitor method for Break and Continue nodes to check that they only occur inside of a loop.
 */
void AnalysisVisitor_check_break_continue (NodeVisitor *visitor, ASTNode *node)
{
    /* traverse up the tree until finding a whileloop symbol table or program symbol table */
    ASTNode *parentNode = node;
    while (parentNode->type != PROGRAM) { // stop at program to prevent infinite loop
        if (parentNode->type == WHILELOOP) {
            break;
        }
        parentNode = (ASTNode *)ASTNode_get_attribute (parentNode, "parent");
    }

    if (parentNode->type != WHILELOOP) { // break/continue not inside a while loop
        char *name;
        switch (node->type) {
            case BREAKSTMT: name = "break"; break;
            case CONTINUESTMT: name = "continue"; break;
            default: break;
        }
        ErrorList_printf (ERROR_LIST, "Invalid '%s' outside loop on line %d",
            name, node->source_line);
    }
}

/**
 * @brief type checking for VOID VarDecl nodes, reject 
 * all programs where node's type is void
 */
void AnalysisVisitor_check_vardecl(NodeVisitor* visitor, ASTNode* node)
{
    if (node->vardecl.type == VOID)
    {
        ErrorList_printf(ERROR_LIST, "Invalid 'void' variable '%s' on line '%d'",
        node->vardecl.name, node->source_line);
    }

    // reject arrays of size 0
    if (node->vardecl.is_array && node->vardecl.array_length == 0)
    {
        ErrorList_printf(ERROR_LIST, "Invalid array variable '%s' of size 0 on line '%d'", node->vardecl.name, node->source_line);
    }

    // do not allow values to have name 'main'
    char name[MAX_ID_LEN];
    snprintf(name, MAX_ID_LEN, "%s", node->vardecl.name);
    if (strcmp(name, "main") == 0)
    {
        ErrorList_printf(ERROR_LIST, "Invalid variable with name 'main' on line '%d'", node->source_line);
    }

    // do not allow duplicate variable names (regardless of type)
}

void AnalysisVisitor_check_program(NodeVisitor* visitor, ASTNode* tree)
{
    // check for null tree
    // still need the handle edge cases!'
    if (lookup_symbol(tree, "main") == NULL)
    {
        ErrorList_printf(ERROR_LIST, "No main function found in program");
    }

    // TODO: handle 'main' edge cases!

    // make sure no parameters in main (funcdecl postvisit)
    // make sure main returns an int (return type postvisit)

    // TODO: check that nothing else is in the global scope that shouldn't be
}

ErrorList* analyze (ASTNode* tree)
{
    /* allocate analysis structures */
    NodeVisitor* v = NodeVisitor_new();
    v->data = (void*)AnalysisData_new();
    v->dtor = (Destructor)AnalysisData_free;

    // ---use previsit for "check" and postvisit for "infer"---
    
    v->previsit_program = AnalysisVisitor_check_program;
    v->previsit_vardecl = AnalysisVisitor_check_vardecl; // change to previsit later
    v->previsit_location = AnalysisVisitor_check_location;
    v->previsit_break = AnalysisVisitor_check_break_continue;
    v->previsit_continue = AnalysisVisitor_check_break_continue;
    v->previsit_return = AnalysisVisitor_check_return;
    v->previsit_assignment = AnalysisVisitor_check_assignment;
    

    /* BOILERPLATE: TODO: register analysis callbacks */
    //-------------------C Tests---------------------
    // Reject any type mismatch errors that require only literal and location inference [ ] ** 
    // Reject arrays of size zero [ X ]
    // Reject 'return' statements that do not match their function's return type [ ] ** 
    // Reject 'break' and 'continue' statements outside a loop [ X ]
    //----------------------------------------------


    /* perform analysis, save error list, clean up, and return errors */
    NodeVisitor_traverse(v, tree);
    ErrorList* errors = ((AnalysisData*)v->data)->errors;
    NodeVisitor_free(v);
    return errors;
}

