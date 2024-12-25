/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 *
 * AI Assist Statement: AI generated test cases.
 *
 * Formatted with clang LLVM standard.
 *
 * @authors Declan McCue, Ben McCray
 */

#include "p2-parser.h"

/* Prototype the EBNF function headers so they follow the reference order when
 * implemented. */
ASTNode *parse_program(TokenQueue *);
ASTNode *parse_variable_declaration(TokenQueue *);
ASTNode *parse_function_declaration(TokenQueue *);
void parse_function_parameters(TokenQueue *, ParameterList *);
ASTNode *parse_braced_block(TokenQueue *);
ASTNode *parse_statement(TokenQueue *);
ASTNode *parse_expression(TokenQueue *);
ASTNode *parse_expression_binary(TokenQueue *, uint8_t);
ASTNode *parse_expression_unary(TokenQueue *);
ASTNode *parse_expression_base(TokenQueue *);
void parse_function_call_args(TokenQueue *, struct NodeList *);
ASTNode *parse_literal(TokenQueue *);

/**
 * @brief Look up the source line of the next token in the queue.
 *
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line(TokenQueue *input) {
  if (TokenQueue_is_empty(input)) {
    Error_throw_printf("Unexpected end of input\n");
  }
  return TokenQueue_peek(input)->line;
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
void match_and_discard_next_token(TokenQueue *input, TokenType type,
                                  const char *text) {
  if (TokenQueue_is_empty(input)) {
    Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != type || !token_str_eq(token->text, text)) {
    Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n", text,
                       token->text, get_next_token_line(input));
  }
  Token_free(token);
}

/**
 * @brief Remove next token from the queue
 *
 * Throws an error if there are no more tokens.
 *
 * @param input Token queue to modify
 */
void discard_next_token(TokenQueue *input) {
  if (TokenQueue_is_empty(input)) {
    Error_throw_printf("Unexpected end of input\n");
  }
  Token_free(TokenQueue_remove(input));
}

/**
 * @brief Look ahead at the type of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */
bool check_next_token_type(TokenQueue *input, TokenType type) {
  if (TokenQueue_is_empty(input)) {
    return false;
  }
  Token *token = TokenQueue_peek(input);
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
bool check_next_token(TokenQueue *input, TokenType type, const char *text) {
  if (TokenQueue_is_empty(input)) {
    return false;
  }
  Token *token = TokenQueue_peek(input);
  return (token->type == type) && (token_str_eq(token->text, text));
}

/**
 * @brief Parse and return a Decaf type
 *
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type(TokenQueue *input) {
  if (TokenQueue_is_empty(input)) {
    Error_throw_printf("Unexpected end of input (expected type)\n");
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != KEY) {
    Error_throw_printf("Invalid type '%s' on line %d\n", token->text,
                       get_next_token_line(input));
  }
  DecafType t = VOID;
  if (token_str_eq("int", token->text)) {
    t = INT;
  } else if (token_str_eq("bool", token->text)) {
    t = BOOL;
  } else if (token_str_eq("void", token->text)) {
    t = VOID;
  } else {
    Error_throw_printf("Invalid type '%s' on line %d\n", token->text,
                       get_next_token_line(input));
  }
  Token_free(token);
  return t;
}

/**
 * @brief Parse and return a Decaf identifier
 *
 * @param input Token queue to modify
 * @param buffer String buffer for parsed identifier (should be at least
 * @c MAX_TOKEN_LEN characters long)
 */
void parse_id(TokenQueue *input, char *buffer) {
  if (TokenQueue_is_empty(input)) {
    Error_throw_printf("Unexpected end of input (expected identifier)\n");
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != ID) {
    Error_throw_printf("Invalid ID '%s' on line %d\n", token->text,
                       get_next_token_line(input));
  }
  snprintf(buffer, MAX_ID_LEN, "%s", token->text);
  Token_free(token);
}

ASTNode *parse(TokenQueue *input) {
  if (input == NULL)
    Error_throw_printf("Input token queue pointer was null.\n");
  return parse_program(input);
}

ASTNode *parse_program(TokenQueue *input) {
  NodeList *vars = NodeList_new();
  NodeList *funcs = NodeList_new();

  while (!TokenQueue_is_empty(input)) {

    /* functions need to be checked first, as "def" is technically a KEY, just
     * not a variable key. */
    if (check_next_token(input, KEY, "def")) {
      NodeList_add(funcs, parse_function_declaration(input));
    }

    /* let parse_variable_declaration check the type of the next token. from
       here, just assume that it's okay. */
    else if (check_next_token_type(input, KEY)) {
      NodeList_add(vars, parse_variable_declaration(input));
    }

    /* if it's not a function or global variable, we're dead here. */
    else {
      Error_throw_printf("Unexpected token in program on line %d. Expecting a "
                         "function declaration or a variable declaration.\n",
                         get_next_token_line(input));
    }
  }

  return ProgramNode_new(vars, funcs);
}

/**
 * @brief Parses variable declarations.
 *
 * LL(1) Grammar:
 * VarDecl -> Type ID ('[' DEC ']')? ';'
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_variable_declaration(TokenQueue *input) {
  int source = get_next_token_line(input);
  DecafType type = parse_type(input);
  char id[MAX_ID_LEN];
  parse_id(input, id);
  bool is_array = false;
  int arr_len = 1;

  /* array type */
  if (check_next_token(input, SYM, "[")) {
    is_array = true;
    match_and_discard_next_token(input, SYM, "[");

    /* can't use discard or match here since it will free the token, and destroy
     * our dec-lit->type call */
    Token *dec_lit = TokenQueue_remove(input);

    if (dec_lit->type != DECLIT)
      Error_throw_printf("Invalid array size on line %d\n", source);
    arr_len = (int)strtol(dec_lit->text, NULL, 0);

    Token_free(dec_lit);

    match_and_discard_next_token(input, SYM, "]");
  }

  match_and_discard_next_token(input, SYM, ";");

  return VarDeclNode_new(id, type, is_array, arr_len, source);
}

/**
 * @brief Parses function declarations.
 *
 * LL(1) Grammar:
 * FuncDecl -> def Type ID '(' Params? ')' Block
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_function_declaration(TokenQueue *input) {
  int source = get_next_token_line(input);
  match_and_discard_next_token(input, KEY, "def");

  DecafType type = parse_type(input);
  char id[MAX_ID_LEN];
  parse_id(input, id);

  match_and_discard_next_token(input, SYM, "(");

  ParameterList *params = ParameterList_new();
  if (!check_next_token(input, SYM, ")"))
    parse_function_parameters(input, params);

  match_and_discard_next_token(input, SYM, ")");

  ASTNode *body = parse_braced_block(input);

  return FuncDeclNode_new(id, type, params, body, source);
}

/**
 * @brief Parses function calls, similar to parse_function_call_args...
 *
 * LL(1) Grammar:
 * Params -> Type ID (',' Type ID )*
 *
 * @param input Token queue to examine
 * @param params Pointer to the parameter list to be populated
 * @returns ParameterList *
 */
void parse_function_parameters(TokenQueue *input, ParameterList *params) {
  if (TokenQueue_is_empty(input))
    Error_throw_printf("Unexpected end of input (function parameters)\n");
  if (params == NULL)
    Error_throw_printf("Internal error - parameters passed was null. "
                       "(parse_function_parameters)");

  /* check_next_token internally checks if empty, this is safe. */
  do {
    DecafType type = parse_type(input);
    char id[MAX_ID_LEN];
    parse_id(input, id);

    ParameterList_add_new(params, id, type);

    if (check_next_token(input, SYM, ","))
      match_and_discard_next_token(input, SYM, ",");
    else
      break;

  } while (0xdecaf); /* lol */
}

/**
 * @brief Parses block bracing for scope.
 *
 * LL(1) Grammar:
 * Block -> '{' VarDecl* Stmt* '}'
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_braced_block(TokenQueue *input) {
  int source = get_next_token_line(input);

  NodeList *vars = NodeList_new();
  NodeList *stmts = NodeList_new();

  match_and_discard_next_token(input, SYM, "{");

  while (true) {
    if (check_next_token(input, KEY, "int") ||
        check_next_token(input, KEY, "bool") ||
        check_next_token(input, KEY, "void"))
      NodeList_add(vars, parse_variable_declaration(input));
    else
      break;
  }

  while (true) {
    if (!TokenQueue_is_empty(input)) {
      if (check_next_token(input, SYM, "}")) {
        break;
      }
    }
    NodeList_add(stmts, parse_statement(input));
  }

  match_and_discard_next_token(input, SYM, "}");

  return BlockNode_new(vars, stmts, source);
}

/**
 * @brief Parses statements.
 *
 * LL(1) Grammar:
 * Stmt -> ID Stmt'
 *       |  if '(' Expr ')' Block (else Block)?
 *       |  while '(' Expr ')' Block
 *       |  return Expr? ';'
 *       |  break ';'
 *       |  continue ';'
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_statement(TokenQueue *input) {
  int source = get_next_token_line(input);

  if (check_next_token(input, KEY, "if")) {
    match_and_discard_next_token(input, KEY, "if");

    /* primary conditional */
    match_and_discard_next_token(input, SYM, "(");
    ASTNode *cond = parse_expression(input);
    match_and_discard_next_token(input, SYM, ")");

    /* statement body */
    ASTNode *body = parse_braced_block(input);

    /* optional (0-1) else statement. can be NULL. */
    ASTNode *else_body = NULL;
    if (check_next_token(input, KEY, "else")) {
      match_and_discard_next_token(input, KEY, "else");
      else_body = parse_braced_block(input);
    }

    return ConditionalNode_new(cond, body, else_body, source);
  }

  else if (check_next_token(input, KEY, "while")) {
    match_and_discard_next_token(input, KEY, "while");
    match_and_discard_next_token(input, SYM, "(");
    ASTNode *condition = parse_expression(input);
    match_and_discard_next_token(input, SYM, ")");
    ASTNode *body = parse_braced_block(input);
    return WhileLoopNode_new(condition, body, source);
  }

  else if (check_next_token(input, KEY, "return")) {
    match_and_discard_next_token(input, KEY, "return");

    /* can be NULL */
    ASTNode *expr = NULL;
    if (!check_next_token(input, SYM, ";"))
      expr = parse_expression(input);

    match_and_discard_next_token(input, SYM, ";");
    return ReturnNode_new(expr, source);
  }

  else if (check_next_token(input, KEY, "break")) {
    match_and_discard_next_token(input, KEY, "break");
    match_and_discard_next_token(input, SYM, ";");
    return BreakNode_new(source);
  }

  else if (check_next_token(input, KEY, "continue")) {
    match_and_discard_next_token(input, KEY, "continue");
    match_and_discard_next_token(input, SYM, ";");
    return ContinueNode_new(source);
  }

  /* Loc and FuncCall */
  else if (check_next_token_type(input, ID)) {
    char id[MAX_ID_LEN];
    parse_id(input, id);

    /* funccall */
    /* FuncCall -> ID '(' Args? ')' */
    if (check_next_token(input, SYM, "(")) {
      match_and_discard_next_token(input, SYM, "(");

      struct NodeList *args = NodeList_new();
      if (!check_next_token(input, SYM, ")"))
        parse_function_call_args(input, args);

      match_and_discard_next_token(input, SYM, ")");
      match_and_discard_next_token(input, SYM, ";");

      return FuncCallNode_new(id, args, source);
    }

    /* loc, we'd hope, or just fail who cares */
    /* Loc -> ID ('[' Expr ']')? */
    else {
      /* NULL is allowed when no internal expression */
      ASTNode *internal_expr = NULL;

      if (check_next_token(input, SYM, "[")) {
        match_and_discard_next_token(input, SYM, "[");
        internal_expr = parse_expression(input);
        match_and_discard_next_token(input, SYM, "]");
      }

      ASTNode *loc = LocationNode_new(id, internal_expr, source);
      /* end loc code copy */

      match_and_discard_next_token(input, SYM, "=");
      ASTNode *assignee_expr = parse_expression(input);
      match_and_discard_next_token(input, SYM, ";");
      return AssignmentNode_new(loc, assignee_expr, source);
    }
  }

  Error_throw_printf("Invalid statement on line %d\n", source);
  return NULL;
}

/**
 * @brief Parses general expressions (a parent of sorts).
 *
 * LL(1) Grammar:
 * Expr -> BinExpr (haha. useless.)
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_expression(TokenQueue *input) {
  if (TokenQueue_is_empty(input))
    Error_throw_printf(
        "Unexpected end of input when trying to parse an expression.\n");
  return parse_expression_binary(input, 0);
}

/**
 * @brief Parses binary expressions.
 *
 * LL(1) Grammar:
 * BinExpr -> UnaryExpr BinExpr'
 *
 * BinExpr' -> \0
 *           | BINOP UnaryExpr BinExpr'
 *
 * @param input Token queue to examine
 * @param precedence recursive operation precedence level
 * @returns ASTNode *
 */
ASTNode *parse_expression_binary(TokenQueue *input, uint8_t precedence_level) {
  int source = get_next_token_line(input);
  if (precedence_level == 6) /* unary precedence level */
    return parse_expression_unary(input);

  /* traditional left assiocation */
  ASTNode *left = parse_expression_binary(input, precedence_level + 1);

  /* repeat concurrent operators at the same precedence level */
  while (!TokenQueue_is_empty(input)) {
    BinaryOpType operator;

    if (precedence_level == 0 && check_next_token(input, SYM, "||")) {
      operator= OROP;
    } else if (precedence_level == 1 && check_next_token(input, SYM, "&&")) {
      operator= ANDOP;
    } else if (precedence_level == 2 && (check_next_token(input, SYM, "==") ||
                                         check_next_token(input, SYM, "!="))) {
      operator= check_next_token(input, SYM, "==") ? EQOP : NEQOP;
    } else if (precedence_level == 3 && (check_next_token(input, SYM, "<") ||
                                         check_next_token(input, SYM, "<=") ||
                                         check_next_token(input, SYM, ">") ||
                                         check_next_token(input, SYM, ">="))) {
      if (check_next_token(input, SYM, "<="))
        operator= LEOP;
      else if (check_next_token(input, SYM, ">="))
        operator= GEOP;
      else if (check_next_token(input, SYM, "<"))
        operator= LTOP;
      else
        operator= GTOP;
    } else if (precedence_level == 4 && (check_next_token(input, SYM, "+") ||
                                         check_next_token(input, SYM, "-"))) {
      operator= check_next_token(input, SYM, "+") ? ADDOP : SUBOP;
    } else if (precedence_level == 5 && (check_next_token(input, SYM, "*") ||
                                         check_next_token(input, SYM, "/") ||
                                         check_next_token(input, SYM, "%"))) {
      if (check_next_token(input, SYM, "*"))
        operator= MULOP;
      else if (check_next_token(input, SYM, "/"))
        operator= DIVOP;
      else
        operator= MODOP;
    } else {
      /* no longer a binary operator, let the compiler fail elsewhere. */
      break;
    }

    /* operator determined by the monster if statement */
    discard_next_token(input);

    /* we know to check higher if we've consumed all the current level */
    ASTNode *right = parse_expression_binary(input, precedence_level + 1);

    /* keep nesting left in itself to continue repeated precedence operations */
    left = BinaryOpNode_new(operator, left, right, source);
  }

  return left;
}

/**
 * @brief Parses unary expressions.
 *
 * LL(1) Grammar:
 * UnaryExpr -> UNOP BaseExpr
 *            |  BaseExpr
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_expression_unary(TokenQueue *input) {
  int source = get_next_token_line(input);

  if (check_next_token(input, SYM, "-") || check_next_token(input, SYM, "!")) {
    UnaryOpType type = check_next_token(input, SYM, "-") ? NEGOP : NOTOP;
    discard_next_token(input);
    return UnaryOpNode_new(type, parse_expression_base(input), source);
  } else {
    return parse_expression_base(input);
  }
}

/**
 * @brief Parses expressions.
 *
 * LL(1) Grammar:
 * BaseExpr -> '(' Expr ')'
 *           | Id BaseExpr'
 *           | Lit
 * BaseExpr' -> '=' Expr
 *            | '(' Args? ')'
 *
 *
 * @param input Token queue to examine
 * @returns ASTNode *
 */
ASTNode *parse_expression_base(TokenQueue *input) {
  int source = get_next_token_line(input);

  /* enclosed expressions */
  if (check_next_token(input, SYM, "(")) {
    match_and_discard_next_token(input, SYM, "(");
    ASTNode *expr = parse_expression(input);
    match_and_discard_next_token(input, SYM, ")");
    return expr;
  }

  /* literals - some redundancy, maybe there is an easier alternative for
     literals. see parse_literal. */
  else if (check_next_token_type(input, HEXLIT) ||
           check_next_token_type(input, DECLIT) ||
           check_next_token_type(input, STRLIT) ||
           strncmp("true", TokenQueue_peek(input)->text, 5) == 0 ||
           strncmp("false", TokenQueue_peek(input)->text, 6) == 0) {
    ASTNode *literal = parse_literal(input);
    discard_next_token(input);
    return literal;
  }
  /* ID BaseExpr' */
  else if (check_next_token_type(input, ID)) {
    char id[MAX_ID_LEN];
    parse_id(input, id);

    /* funccall */
    /* FuncCall -> ID '(' Args? ')' */
    if (check_next_token(input, SYM, "(")) {
      match_and_discard_next_token(input, SYM, "(");

      struct NodeList *args = NodeList_new();
      if (!check_next_token(input, SYM, ")"))
        parse_function_call_args(input, args);

      match_and_discard_next_token(input, SYM, ")");

      return FuncCallNode_new(id, args, source);
    }

    /* loc, we'd hope, or just fail who cares */
    /* FuncCall -> ID '(' Args? ')' */
    else {
      /* NULL is allowed when no internal expression */
      ASTNode *internal_expr = NULL;

      if (check_next_token(input, SYM, "[")) {
        match_and_discard_next_token(input, SYM, "[");
        internal_expr = parse_expression(input);
        match_and_discard_next_token(input, SYM, "]");
      }

      return LocationNode_new(id, internal_expr, source);
    }
  }

  Error_throw_printf("Invalid base expression on line: %d\n", source);
  return NULL;
}

/**
 * @brief Builds NodeList struct of arguments for function calls.
 *
 * LL(1) Grammar:
 * Args -> Expr (',' Expr)*
 *
 * @param input Token queue to examine
 * @param args Pointer to the args list that needs to be populated
 * @returns function arguments (struct NodeList *args)
 */
void parse_function_call_args(TokenQueue *input, struct NodeList *args) {
  if (TokenQueue_is_empty(input))
    Error_throw_printf("Unexpected end of input when trying to parse function "
                       "call arguments.\n");
  if (args == NULL)
    Error_throw_printf(
        "Internal error - args passed was null. (parse_function_call_args)");

  do {
    NodeList_add(args, parse_expression(input));

    if (check_next_token(input, SYM, ","))
      match_and_discard_next_token(input, SYM, ",");
    else
      break;

  } while (0xdecaf); /* lol */
}

/**
 * @brief Unescapes characters in strings.
 *
 * @param token The current token (a token string).
 * @returns A String LiteralNode
 */
struct ASTNode *format_string(Token *token) {
  if (token == NULL)
    Error_throw_printf("internal error - unexpexted null token\n");

  char buf[MAX_LINE_LEN];
  const char *src = token->text;
  char *dst = buf;

  /* skip the first string escape */
  src++;

  while (*src != '\0') {
    if (*src != '\\') {
      *dst++ = *src;
      src++;
      continue;
    }

    /* skip the escape code */
    src++;

    switch (*src) {
    case 'n':
      *dst++ = '\n';
      break;
    case 't':
      *dst++ = '\t';
      break;
    case '\\':
      *dst++ = '\\';
      break;
    case '\"':
      *dst++ = '\"';
      break;
    default:
      *dst++ = '\\';
      *dst++ = *src;
      break;
    }

    src++;
  }

  /* there's a \" hiding at the end of the string, get rid of it. */
  *(dst - 1) = '\0';
  return LiteralNode_new_string(buf, token->line);
}

/**
 * @brief Parses the literal type out of a token.
 *
 * @param input Token queue to examine
 * @returns The appropriate LiteralNode type.
 */
ASTNode *parse_literal(TokenQueue *input) {
  if (TokenQueue_is_empty(input))
    Error_throw_printf(
        "Unexpected end of input. Expecting a literal to parse.\n");

  Token *token = TokenQueue_peek(input);
  switch (token->type) {
  case DECLIT:
  case HEXLIT:
    return LiteralNode_new_int(strtol(token->text, NULL, 0), token->line);
  case STRLIT:
    return format_string(token);
  default: {

    /* hopefully it's a boolean! */
    bool is_true;
    if ((is_true = (strncmp("true", token->text, 5) == 0)) ||
        strncmp("false", token->text, 6) == 0) {
      return LiteralNode_new_bool(is_true, token->line);
    }
  }
  }
  Error_throw_printf("Unsupported type when parsing literal. Line: %d\n",
                     token->line);
  return NULL;
}