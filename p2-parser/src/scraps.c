/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * @author Gillian Kelly
 */

#include "p2-parser.h"


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
  if (TokenQueue_is_empty (input))
    {
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
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected \'%s\')\n", text);
    }
  Token *token = TokenQueue_remove (input);
  if (token->type != type || !token_str_eq (token->text, text))
    {
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

void discard_next_token(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
    {
      Error_throw_printf("Unexpected end of input\n");
    }
  Token_free (TokenQueue_remove(input));
}

/**
 * @brief Look ahead at the type of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */

bool check_next_token_type(TokenQueue *input, TokenType type)
{
  if (TokenQueue_is_empty(input))
    {
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

bool check_next_token (TokenQueue *input, TokenType type, const char *text)
{
  if (TokenQueue_is_empty (input))
    {
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
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected type)\n");
    }
  Token *token = TokenQueue_remove (input);
  if (token->type != KEY)
    {
      Error_throw_printf ("Invalid type '%s' on line %d\n", token->text, get_next_token_line (input));
    }

  DecafType t = VOID;
  if (token_str_eq ("int", token->text))
    {
      t = INT;
    }
  else if (token_str_eq ("bool", token->text))
    {
      t = BOOL;
    }
  else if (token_str_eq ("void", token->text))
    {
      t = VOID;
    }
  else
    {
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

void parse_id(TokenQueue *input, char *buffer)
{
  if (TokenQueue_is_empty (input))
    {
      Error_throw_printf ("Unexpected end of input (expected identifier)\n");
    }
  Token *token = TokenQueue_remove (input);
  if (token->type != ID)
    {
      Error_throw_printf ("Invalid ID '%s' on line %d\n", token->text,
                          get_next_token_line (input));
    }
  snprintf (buffer, MAX_ID_LEN, "%s", token->text);
  Token_free (token);
}

/**
 * @brief Parse and return a LiteralNode for an Assignment.
 */
ASTNode* parse_literal(TokenQueue *input)
{
    int source_line = get_next_token_line(input);
  // check for a decimal literal
  if (check_next_token_type(input, DECLIT))
    {
      Token *token = TokenQueue_remove(input);
      int value = atoi(token->text); // convert string to integer
      Token_free(token);
      return LiteralNode_new_int(value, source_line);
    }
  // check for a hexadecimal literal
  else if (check_next_token_type(input, HEXLIT))
    {
      Token *token = TokenQueue_remove(input);
      int value = strtol(token->text, NULL, 16); // convert hex string to integer
      Token_free(token);
      return LiteralNode_new_int(value, source_line);
    }
  // check for a string literal
  else if (check_next_token_type(input, STRLIT))
    {
      Token *token = TokenQueue_remove(input);
      // remove leading a trailing quotes
      char value[MAX_ID_LEN];
      strcpy(value, token->text);
      char quote_char = '\"';
      int i, j;
      int len = strlen(value);
      for (i = j = 0; i < len; i++)
      {
        if (value[i] != quote_char)
        {
            value[j++] = value[i];
        }
      }
      printf("%s\n", value);
      return LiteralNode_new_string(value, source_line);
      Token_free(token);
    }

  else
    {
      Token *t = TokenQueue_remove(input);
      Error_throw_printf ("Expected literal but found '%s' else on line %d\n", t->type , source_line);
      Token_free(t);
      return NULL; // this line will never be reached due to Error_throw_printf
    }

}

/**
 * @brief redirects all statements to appropriate methods
 */
ASTNode* parse_statement(TokenQueue* input)
{
    ASTNode *statement;
    // check for KEYWORDS: if, while, return, break, continue
    if (check_next_token_type(input, KEY))
    {
        if (check_next_token(input, KEY, "break"))
        {
           return parse_break(input);
        }
        else if (check_next_token(input, KEY, "continue"))
        {
           return parse_continue(input);
        }
        else if (check_next_token(input, KEY, "return"))
        {
            return parse_return(input);
        }
    }
    // check for ID (LocationNode and FuncCall start with ID)
    if (check_next_token_type(input, ID))
    {
        // determine if its a LocationNode or a FuncCall
        char name[MAX_ID_LEN];
        Token *token = TokenQueue_peek(input);
        
        // FuncCall ID is followed by '('
        if (token->type == ID)
        {
           TokenQueue_remove(token);
           Token *next_token = TokenQueue_peek(input);
           if (next_token->type == SYM)
           {
            TokenQueue_add(input, token); // add removed token BACK to queue
            parse_func_call(input);
           }

               
           parse_loc(input);
        } 
    }


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
    // int source_line = get_next_token_line(input);
    ASTNode* statement;
  if (check_next_token (input, KEY, "break"))
    {
      discard_next_token(input); // consume 'break'
      match_and_discard_next_token(input, SYM, ";");
      return BreakNode_new(get_next_token_line(input));
    }
  else if (check_next_token (input, KEY, "continue"))
    {
      discard_next_token(input); // consume 'continue'
      match_and_discard_next_token(input, SYM, ";");
      return ContinueNode_new(get_next_token_line(input));
    }
  else if (check_next_token (input, KEY, "return"))
    {
      statement = parse_return(input);
      return statement;
    }
  else if (check_next_token_type(input, ID))
    {
      // handle assignments, function calls, or other expressions that start
      // with an ID
      
      return parse_assignment(input);
    }
  else if (check_next_token_type(input, DECLIT)
           || check_next_token_type(input, HEXLIT)
           || check_next_token_type(input, STRLIT))
    {
      // handle literals as standalone expressions (e.g., "42;" or
      // "\"string\";")
      ASTNode* literal_node = parse_literal(input);
      
      match_and_discard_next_token(input, SYM, ";");
      return literal_node;
    }
  else
    {
      // if no known statement type is recognized, throw an error
      Error_throw_printf("Unexpected statement type %s on line %d", get_next_token_line(input));
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

  int source_line = get_next_token_line (input);
  NodeList *vars = NodeList_new ();
  NodeList *stmts = NodeList_new ();

  // make sure starting with left curly brace
  if (!check_next_token (input, SYM, "{"))
    {
      Error_throw_printf ("BlockNodes must start with '{'\n");
    }

  match_and_discard_next_token (input, SYM, "{");

  // parse contents of block
  // make sure VarDecls appear BEFORE other statements
  while (check_next_token_type(input, INT) || check_next_token_type(input, BOOL))
  {
    NodeList_add(vars, parse_vardecl(input));
  }
  
  // parse statements
    if (check_next_token_type (input, KEY))
      {
        // need to check for following keywords:
        // "return", "break", "continue", "while"
        if (check_next_token (input, KEY, "return"))
          {
              NodeList_add (stmts, parse_return(input));
          }
        else if (check_next_token (input, KEY, "break"))
          {
              NodeList_add(stmts, parse_break(input));
          }
          else if (check_next_token(input, KEY, "continue"))
          {
              NodeList_add(stmts, parse_continue(input));
          }
      }

    // LocationNode starts with an ID
    if (check_next_token_type(input, ID))
    {
      parse_loc(input);
    }

  // if (!check_next_token (input, SYM, "}"))
  //   {
  //     Error_throw_printf ("Missing ending curly braces on line %d'\n", source_line);
  //   }

  // make sure ends with right curly brace
  match_and_discard_next_token (input, SYM, "}");

  return BlockNode_new (vars, stmts, source_line);
}



/**
 * @brief Parse and return a BinaryOp node
 */

// TODO: Add support
ASTNode* parse_bin_exp(TokenQueue* input)
{
    int source_line = get_next_token_line(input);
    ASTNode* left;
    ASTNode* right;
    BinaryOpType operator;

    // parse left
    if (check_next_token_type(input, LITERAL))
    {
        left = parse_literal(input);
    } 
    else if (check_next_token_type(input, ID))
    {
        char name[MAX_ID_LEN];
        parse_id(input, name);
    }

    // consume operator
    Token *t = TokenQueue_remove(input);
    if (t->text == BinaryOpToString(EQOP))
    {
        operator = EQOP;
    }

    // parse right:
    if (check_next_token_type(input, LITERAL))
    {
        right = parse_literal(input);
    }

    return BinaryOpNode_new(operator, left, right, source_line);
}