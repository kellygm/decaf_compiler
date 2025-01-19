/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * @author Gillian Kelly
 */

#include "p2-parser.h"

ASTNode *parse_program(TokenQueue *input);

ASTNode *parse_block (TokenQueue *input);
ASTNode *parse_return (TokenQueue *input);
ASTNode *parse_continue (TokenQueue *input);

ASTNode *parse_expression (TokenQueue *input);
ASTNode *parse_bin_expr (TokenQueue *input, ASTNode *left);
ASTNode *parse_unary_expr (TokenQueue *input);
ASTNode *parse_base_expr (TokenQueue *input);

ASTNode *parse_statement (TokenQueue *input);
ASTNode *parse_literal (TokenQueue *input);
ASTNode *parse_assignment (TokenQueue *input);
ASTNode *parse_loc (TokenQueue *input);
ASTNode *parse_conditional (TokenQueue *input);
ASTNode *parse_func_call (TokenQueue *input);

/*
 * helper functions
 */

/**
 * @brief Look up the source line of the next token in the queue.
 *
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line (TokenQueue *input)
{
  if (TokenQueue_is_empty (input)) {
    Error_throw_printf ("Unexpected end of input\n");
  }
  return TokenQueue_peek (input)->line;
}

/**
 * @brief Check next token for a particular type and text and discard it
 *
 * Throws an error if there are no more tokens or if the next token in the
 * queue does not match the given type or text.
 *
 * @param input Token queue to modify
 * @param type Expected type of next token
 * @param text Expected text of next token
 */
void match_and_discard_next_token (TokenQueue *input, TokenType type, const char *text)
{
  if (TokenQueue_is_empty (input)) {
    Error_throw_printf ("Unexpected end of input (expected \'%s\')\n", text);
  }
  Token *token = TokenQueue_remove (input);
  if (token->type != type || !token_str_eq (token->text, text)) {
    Error_throw_printf ("Expected \'%s\' but found '%s' on line %d\n", text, token->text, get_next_token_line (input));
  }
  Token_free (token);
}

/**
 * @brief Remove next token from the queue
 *
 * Throws an error if there are no more tokens.
 *
 * @param input Token queue to modify
 */

void discard_next_token (TokenQueue *input)
{
  if (TokenQueue_is_empty (input)) {
    Error_throw_printf ("Unexpected end of input\n");
  }
  Token_free (TokenQueue_remove (input));
}

/**
 * @brief Look ahead at the type of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */

bool check_next_token_type (TokenQueue *input, TokenType type)
{
  if (TokenQueue_is_empty (input)) {
    return false;
  }
  Token *token = TokenQueue_peek (input);
  return (token->type == type);
}

/**
 * @brief Look ahead at the type and text of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @param text Expected text of next token
 * @returns True if the next token is of the expected type and text, false if
 * not
 */
bool check_next_token (TokenQueue *input, TokenType type, const char *text)
{
  if (TokenQueue_is_empty (input)){
    return false;
  }
  Token *token = TokenQueue_peek (input);
  return (token->type == type) && (token_str_eq (token->text, text));
}

/**
 * @brief Parse and return a Decaf type
 *
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type (TokenQueue *input)
{
  if (TokenQueue_is_empty (input)) {
    Error_throw_printf ("Unexpected end of input (expected type)\n");
  }
  Token *token = TokenQueue_remove (input);
  if (token->type != KEY) {
    Error_throw_printf ("Invalid type '%s' on line %d\n", token->text, get_next_token_line (input));
  }
  DecafType t = VOID;
  if (token_str_eq ("int", token->text)) {
    t = INT;
  }
  else if (token_str_eq ("bool", token->text)) {
    t = BOOL;
  }
  else if (token_str_eq ("void", token->text)) {
    t = VOID;
  }
  else {
    Error_throw_printf ("Invalid type '%s' on line %d\n", token->text, get_next_token_line (input));
  }

  Token_free (token);
  return t;
}

/**
 * @brief Parse and return a Decaf identifier
 *
 * @param input Token queue to modify
 * @param buffer String buffer for parsed identifier (should be at least
 * @c MAX_TOKEN_LEN characters long)
 */
void parse_id (TokenQueue *input, char *buffer)
{
  if (TokenQueue_is_empty (input)) {
    Error_throw_printf ("Unexpected end of input (expected identifier) on line %d\n", get_next_token_line(input));
  }

  Token *token = TokenQueue_remove (input);
  
  if (token->type != ID) {
    Error_throw_printf ("Invalid ID '%s' on line %d\n", token->text, get_next_token_line (input));
  }

  snprintf (buffer, MAX_ID_LEN, "%s", token->text);
  Token_free (token);
}

/**
 * @brief Parse and return a LiteralNode for an Assignment.
 */
ASTNode* parse_literal (TokenQueue *input)
{
  int source_line = get_next_token_line (input);

  // check for a decimal literal
  if (check_next_token_type (input, DECLIT)) {
    Token *token = TokenQueue_remove (input);
    int value = strtol (token->text, NULL, 10); // convert string to integer
    Token_free (token);
    return LiteralNode_new_int (value, source_line);
  }

  // check for a hexadecimal literal
  else if (check_next_token_type (input, HEXLIT)) {
    Token *token = TokenQueue_remove (input);
    // convert hex string to integer
    int value = strtol (token->text, NULL, 16);
    Token_free (token);
    return LiteralNode_new_int (value, source_line);
  }

  // check for boolean literal
  else if (check_next_token (input, KEY, "true") || check_next_token (input, KEY, "false")) {
    Token *token = TokenQueue_remove (input);
    bool value = token_str_eq (token->text, "true") ? true : false;
    Token_free (token);
    return LiteralNode_new_bool (value, source_line);
  }

  // check for a string literal
  // TODO: make sure STRING isn't true or false
  else if (check_next_token_type (input, STRLIT)) {
    Token *token = TokenQueue_remove (input);
    // remove leading and trailing quotes from string literal
    char buffer[MAX_ID_LEN];
    snprintf (buffer, MAX_ID_LEN, "%s", token->text);
    Token_free (token);
    // remove the first and last characters which are quotes
    if (buffer[0] == '"' && buffer[strlen (buffer) - 1] == '"') {
      buffer[strlen (buffer) - 1] = '\0'; // remove last quote
      return LiteralNode_new_string(buffer + 1, source_line); // skip first quote
    }
    else{
      Error_throw_printf ("invalid string literal format on line %d\n", source_line);
    }
    Token_free (token);
  }

  else {
    Token *t = TokenQueue_remove (input);
    Error_throw_printf ("expected literal but found '%s' else on line %d\n", t->text, source_line);
    Token_free (t);
  }
  
  return NULL;
}

/**
 * @brief Parse and return a LocationNode for an Assignment
 * @todo support for array assignments
 */
ASTNode* parse_loc (TokenQueue *input)
{
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected location) on line %d\n", get_next_token_line(input));
    }

  int source_line = get_next_token_line (input);
  char name[MAX_ID_LEN];
  parse_id (input, name);
  struct ASTNode *index = NULL;

  // get index (NULL if non-array element)
  if (check_next_token (input, SYM, "["))
    {
      match_and_discard_next_token (input, SYM, "["); // consume '['
      index = parse_expression(input);
      if (!check_next_token(input, SYM, "]"))
      {
        Error_throw_printf("Unexpected end of input, expected ']' on line %d\n", source_line);
      }
      match_and_discard_next_token (input, SYM, "]");
    }
  return LocationNode_new (name, index, source_line);
}

/**
 * @brief parse and return BaseExpressions
 */
ASTNode* parse_base_expr (TokenQueue *input)
{
  // parse Literals
  ASTNode *node;
  if (check_next_token_type (input, DECLIT)
      || check_next_token_type (input, STRLIT)
      || check_next_token_type (input, HEXLIT)
      || check_next_token (input, KEY, "true")
      || check_next_token (input, KEY, "false"))
    {
      node = parse_literal (input);
    }

  // parse Location
  if (check_next_token_type (input, ID))
    {
      // if '(' follows ID then its a FuncCall
      node = parse_loc (input);
    }
  else
    {
      Token *t = TokenQueue_remove(input);
      int source_line = get_next_token_line (input);
      Error_throw_printf ("Unsupport base expression of type %s at line %d", t->type, source_line);
      Token_free(t);
    }

  return node;
}

/**
 * @brief helper method to convert from
 * string to BinaryOpType. Will return 
 * "NULL" if a match was not found
 */
BinaryOpType get_binop (TokenQueue *input)
{
  BinaryOpType operator= (BinaryOpType) NULL;
  if (check_next_token (input, SYM, "||"))
    {
      operator= OROP;
    }
  else if (check_next_token (input, SYM, "&&"))
    {
      operator= ANDOP;
    }
  else if (check_next_token (input, SYM, "=="))
    {
      operator= EQOP;
    }
  else if (check_next_token (input, SYM, "!="))
    {
      operator= NEQOP;
    }
  else if (check_next_token (input, SYM, "<"))
    {
      operator= LTOP;
    }
  else if (check_next_token (input, SYM, "<="))
    {
      operator= LEOP;
    }
  else if (check_next_token (input, SYM, ">"))
    {
      operator= GTOP;
    }
  else if (check_next_token (input, SYM, ">="))
    {
      operator= GEOP;
    }
  else if (check_next_token (input, SYM, "+"))
    {
      operator= ADDOP;
    }
  else if (check_next_token (input, SYM, "-"))
    {
      operator= SUBOP;
    }
  else if (check_next_token (input, SYM, "*"))
    {
      operator= MULOP;
    }
  else if (check_next_token (input, SYM, "/"))
    {
      operator= DIVOP;
    }
  else if (check_next_token (input, SYM, "%"))
    {
      operator= MODOP;
    }

  return operator;
}

/**
 * @brief parse and return a binary expressions
 */
/**
 * @brief Parse and return a binary expression.
 *        This function handles binary operators such as +, -, *, /, and more.
 *
 * @param input Token queue to modify
 * @param left The left-hand side of the binary expression.
 * @returns Parsed AST node for the binary expression.
 */
ASTNode* parse_bin_expr (TokenQueue *input, ASTNode *left)
{
  int source_line = get_next_token_line (input);

  // Check if there's a binary operator present
  BinaryOpType operator= get_binop (input);
  if (operator== (BinaryOpType) NULL)
    {
      // If no valid binary operator is found, return the left operand as is.
      return left;
    }

  // Discard the operator token to move to the next
  discard_next_token (input);

  // Parse the right-hand side expression
  ASTNode *right = parse_expression (input);

  // Create a new BinaryOpNode for this binary operation
  return BinaryOpNode_new (operator, left, right, source_line);
}

/**
 * @brief Parse and return a VarDecl
 */
ASTNode* parse_vardecl (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected variable) on line %d\n", source_line);
    }
  DecafType type = parse_type (input);
  char name[MAX_TOKEN_LEN];
  int size = 1;
  parse_id (input, name);

  bool is_array = false;
  // check for array decl
  if (check_next_token (input, SYM, "["))
    {
      match_and_discard_next_token (input, SYM, "["); // consume '['
      // check if block is empty
      if (!check_next_token (input, SYM, "]"))
        {
          ASTNode *node = parse_expression (input);
          size = node->literal.integer;
        }
      is_array = true;
      match_and_discard_next_token (input, SYM, "]"); // consume ']'
    }
  match_and_discard_next_token (input, SYM, ";");
  return VarDeclNode_new (name, type, is_array, size, source_line);
}

/**
 * @brief parse and return a unary expression node.
 *        supports unary operators such as '-' and '!'
 */
ASTNode* parse_unary_expr (TokenQueue *input)
{
  int source_line = get_next_token_line (input);

  // check for a unary minus (e.g., -2) or logical negation (e.g., !true)
  if (check_next_token (input, SYM, "-"))
    {
      discard_next_token (input);                  // consume '-'
      ASTNode *operand = parse_expression (input); // parse the operand
      return UnaryOpNode_new (NEGOP, operand, source_line);
    }
  else if (check_next_token (input, SYM, "!"))
    {
      discard_next_token (input);                  // consume '!'
      ASTNode *operand = parse_expression (input); // parse the operand
      return UnaryOpNode_new (NOTOP, operand, source_line);
    }
  else
    {
      // if no unary operator is present, return the base expression
      return parse_base_expr (input);
    }
}

/**
 * @brief parse and return a FunctionCall
 */
ASTNode* parse_func_call (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ( "Unexpected end of input (expected funcCall) on line %d", source_line);
    }

  char name[MAX_ID_LEN];
  parse_id (input, name);
  NodeList *args = NodeList_new ();

  // get opening parenthesis
  match_and_discard_next_token (input, SYM, "(");

  // function call has no args
  if (check_next_token (input, SYM, ")"))
    {
      return FuncCallNode_new (name, args, source_line);
    }

  // get first arg
  NodeList_add (args, parse_expression (input));
  // continue to get args while ',' present
  while (check_next_token (input, SYM, ","))
    {
      match_and_discard_next_token (input, SYM, ",");
      NodeList_add (args, parse_expression (input));
    }
  if (!check_next_token(input, SYM, ")"))
  {
    Error_throw_printf("FuncCall missing ending ')' on line %d", source_line);
  }
  match_and_discard_next_token (input, SYM, ")");
  return FuncCallNode_new (name, args, source_line);
}

/**
 * @brief Parse and return an expression.
 *
 * This function parses different types of expressions, such as literals,
 * function calls, or other arithmetic and logical expressions.
 *
 * @param input Token queue to modify
 * @returns Parsed AST node for the expression.
 */
ASTNode* parse_expression (TokenQueue *input)
{
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected expression) on line\n", get_next_token_line(input));
    }

  int source_line = get_next_token_line (input);
  ASTNode *left = NULL;

  // Check for literals first
  if (check_next_token (input, KEY, "false") || check_next_token (input, KEY, "true"))
    {
      left = parse_literal (input);
    }
  else if (check_next_token_type (input, DECLIT) || check_next_token_type (input, HEXLIT) || check_next_token_type (input, STRLIT))
    {
      left = parse_literal (input);
    }
  // Check for unary operations (e.g., -5, !true)
  else if (check_next_token (input, SYM, "-") || check_next_token (input, SYM, "!"))
    {
      left = parse_unary_expr (input); // Parse and return unary expression
    }
  // Check for identifiers (function calls or variable locations)
  else if (check_next_token_type (input, ID))
    {
      left = parse_base_expr (input);
    }
  else
    {
      Error_throw_printf ("Invalid expression start on line %d", source_line);
    }

  return parse_bin_expr (input, left);
}
/**
 * @brief Parse and return an AssignmentNode
 */
ASTNode* parse_assignment (TokenQueue *input)
{
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected assignment) on line %d\n", get_next_token_line(input));
    }
  ASTNode *loc;
  ASTNode *value;
  int source_line = get_next_token_line (input);
  // get the location (left-hand assignment)
  loc = parse_base_expr (input);

  // discard '='
  match_and_discard_next_token (input, SYM, "=");

  // get the value (right-hand assignment)
  value = parse_expression (input);

  // discard ';'
  match_and_discard_next_token (input, SYM, ";");
  return AssignmentNode_new (loc, value, source_line);
}

/**
 * @brief parse and return a ReturnNode
 */
ASTNode* parse_return (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  match_and_discard_next_token (input, KEY, "return");

  ASTNode *value = NULL;
  // check if returning an expression
  if (!check_next_token (input, SYM, ";"))
    {
      value = parse_expression (input); // Parse return expression if present
    }

  match_and_discard_next_token (input, SYM, ";");
  return ReturnNode_new (value, source_line);
}

/**
 * @brief parse and return a BreakNode
 */
ASTNode* parse_break (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  match_and_discard_next_token (input, KEY, "break");
  if (!check_next_token(input, SYM, ";"))
  {
    Error_throw_printf("Incorrect end of break statement on line %d\n",source_line);
  }
  match_and_discard_next_token (input, SYM, ";");
  return BreakNode_new (source_line);
}

/**
 * @brief parse and return ContinueNode
 */
ASTNode* parse_continue (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  match_and_discard_next_token (input, KEY, "continue");
  if (!check_next_token(input, SYM, ";"))
  {
    Error_throw_printf("Unexpected end of continue statment on line %d\n", source_line);
  }
  match_and_discard_next_token (input, SYM, ";");
  return ContinueNode_new (source_line);
}

/**
 * @brief Parse and return a ParameterList [FAILING B TESTS!]
 */
ParameterList* parse_parameter_list (TokenQueue *input)
{
  ParameterList *param_list = ParameterList_new ();
  // immediately return for empty list for no params
  if (check_next_token (input, SYM, ")"))
    {
      return param_list;
    }
  else
    {
      // parse first parameter
      char name[MAX_ID_LEN];
      DecafType type;
      type = parse_type (input);
      parse_id (input, name);
      ParameterList_add_new (param_list, name, type);
      // continue parsing parameters while ',' is present
      while (check_next_token (input, SYM, ","))
        {
          match_and_discard_next_token(input, SYM, ","); // consume ','
          char name[MAX_ID_LEN];
          DecafType type;
          type = parse_type (input);
          parse_id (input, name);
          ParameterList_add_new (param_list, name, type);
        }
    }
  return param_list;
}

ASTNode* parse_while (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  ASTNode *condition;
  ASTNode *body;
  match_and_discard_next_token (input, KEY, "while");
  // get conditions for while loop
  match_and_discard_next_token (input, SYM, "(");
  condition = parse_expression (input);
  match_and_discard_next_token (input, SYM, ")");
  body = parse_block (input);
  return WhileLoopNode_new (condition, body, source_line);
}

ASTNode* parse_conditional (TokenQueue *input)
{
  int source_line = get_next_token_line (input);
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ( "Unexpected end of input (expected if block) on line %d\n,", source_line);
    }
  ASTNode *condition;
  ASTNode *if_block;
  ASTNode *else_block;
  match_and_discard_next_token (input, KEY, "if");
  match_and_discard_next_token (input, SYM, "(");
  condition = parse_expression (input);
  match_and_discard_next_token (input, SYM, ")");
  if (!check_next_token (input, SYM, "{"))
    {
      Error_throw_printf ("Invalid end of conditional node on line %d", source_line);
    }
  if_block = parse_block (input);
  if (check_next_token (input, KEY, "else"))
    {
      match_and_discard_next_token (input, KEY, "else");
      else_block = parse_block (input);
    }
  else
    {
      else_block = NULL;
    }
  return ConditionalNode_new (condition, if_block, else_block, source_line);
}

/**
 * @brief Parse and return a statement.
 *
 * This function checks for different types of statements in the input queue,
 * such as `break`, `continue`, `return`, assignments, and expressions.
 *
 * @param input Token queue to modify
 * @returns The parsed AST node for the statement.
 */
ASTNode* parse_statement (TokenQueue *input)
{
  // retain and use get_next_token_line for error handling if needed
  if (check_next_token (input, KEY, "continue"))
    {
      return parse_continue (input);
    }
  else if (check_next_token (input, KEY, "break"))
    {
      return parse_break (input);
    }
  else if (check_next_token (input, KEY, "return"))
    {
      return parse_return (input);
    }
  else if (check_next_token (input, KEY, "while"))
    {
      return parse_while (input);
    }
  else if (check_next_token (input, KEY, "if"))
    {
      return parse_conditional (input);
    }
  // Check for assignment or expression statements
  else if (check_next_token_type (input, ID))
    {
      // handle identifiers, literals, or expressions
      Token *t = TokenQueue_peek (input);
      if (token_str_eq (t->next->text, "("))
        {
          return parse_func_call (input);
        }
      else
        {
          return parse_assignment (input);
        }
      Token_free (t);
      // return parse_assignment(input);
    }
  else
    {
      Error_throw_printf ("Unexpected statement on line %d", get_next_token_line (input));
    }
  return NULL;
}

/**
 * @brief Parse and return a BlockNode
 */

// TODO:
// 1. ensure varDecl appear BEFORE all statements
// 2. allow multiple "things" in a block
ASTNode* parse_block (TokenQueue *input)
{
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected block)\n");
    }
  // initialize variables
  int source_line = get_next_token_line (input);
  NodeList *vars = NodeList_new ();
  NodeList *stmts = NodeList_new ();

  // make sure starting with left curly brace
  if (!check_next_token (input, SYM, "{"))
    {
      Error_throw_printf ("BlockNodes must start with '{'\n");
    }

  match_and_discard_next_token (input, SYM, "{");

  // parse variable declarations first
  while (check_next_token (input, KEY, "int") || check_next_token (input, KEY, "bool"))
    {
      NodeList_add (vars, parse_vardecl (input));
    }

  // parse statements after all variable declarations
  while (!check_next_token (input, SYM, "}"))
    {
      NodeList_add (stmts, parse_statement (input));
    }

  // make sure ends with right curly brace
  match_and_discard_next_token (input, SYM, "}");

  return BlockNode_new (vars, stmts, source_line);
}

/**
 * @brief Parse and return a FuncDecl
 */
ASTNode* parse_funcdecl (TokenQueue *input)
{
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected function declaration)\n");
    }

  int source_line = get_next_token_line (input);
  match_and_discard_next_token (input, KEY, "def"); // skip over 'def'
  DecafType return_type = parse_type (input);       // get function return type
  ASTNode *body;
  // get function name
  char name[MAX_TOKEN_LEN];
  parse_id (input, name);

  // skip over parenthesis
  match_and_discard_next_token (input, SYM, "(");

  // get the formal parameters
  ParameterList *params = parse_parameter_list (input);

  match_and_discard_next_token (input, SYM, ")");

  // parse the body of the function
  body = parse_block (input);

  // if funcDecl return_type is NOT NULL, make sure RETURN_NODE is present

  // return a new FuncDecl Node with information gathered
  return FuncDeclNode_new (name, return_type, params, body, source_line);
}

/*
 * node-level parsing functions
 */
ASTNode* parse_program (TokenQueue *input) {
  NodeList *vars = NodeList_new ();
  NodeList *funcs = NodeList_new ();

  if (input == NULL)
    {
      Error_throw_printf ("The input can't be null");
    }

  while (!TokenQueue_is_empty(input))
    {
      // peek at next token to determine if adding to 'vars' or 'funcs'
      if (check_next_token (input, KEY, "def")) {
          NodeList_add (funcs, parse_funcdecl (input));
      } 
      else {
        NodeList_add (vars, parse_vardecl (input)); // construct a variable declaration
      }
    }

  return ProgramNode_new (vars, funcs);
}

ASTNode* parse (TokenQueue *input)
{
  return parse_program (input);
}
