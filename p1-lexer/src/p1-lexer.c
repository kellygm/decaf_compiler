/**
 * @file p1-lexer.c
 * @brief Compiler phase 1: lexer
 * @authors Terry Johnson and Gillian Kelly
 * 
 * No AI technology was used 
 */
#include "p1-lexer.h"

TokenQueue *
lex (const char *text)
{
  TokenQueue *tokens = TokenQueue_new ();

  /* compile regular expressions */ 
  Regex *whitespace = Regex_new ("^[ \\n\\t\\r]+"); 
  Regex *letter = Regex_new ("^[a-z]"); 
  Regex *symbols = Regex_new ("^[\\(\\)\\+\\{\\}\\!\\<\\>\\;\\.\\,\\*\\-\\=\\/\\%-]"); // need to add rest of supported symbols -- Terry(believe I did this but might have missed some)
    Regex* multi_symbols = Regex_new("^(<=|>=|==|!=|&&|\\|\\|)");
  Regex *decimals = Regex_new ("^(0|[1-9][0-9]*)");
  Regex *identifiers = Regex_new ("^[a-zA-Z][a-zA-Z_0-9]*"); // changed the + to a * so that there can be single letter identifiers
  Regex *keywords = Regex_new ("^(def|if|else|while|return|break|continue|int|bool|void|true|false)");
  Regex *reserved = Regex_new("^(for|callout|class|interface|extends|implements|new|this|string|float|double|null)");
  Regex *hex_literals = Regex_new ("^(0x[a-fA-F0-9]+)"); 
// Regex *str_literals = Regex_new("^\"([^\n\r\\\\\"]|\\\\[nt\"\\\\])*\""); //"^\"([^\"\\\\]*(\\\\[nt\"\\\\])*[^\"\\\\]*)*\"$" this is wild I don't get it ngl but maybe you will 
  // Regex* comments = Regex_new("^//.*$"); //the . matches any single character except newline so it should match everything after the // up to the newline
                                          //however this won't work for inline comments I don't think because ^// means it expects the //at the beginning of the line.

  Regex *str_literals = Regex_new("^(\"([^\\\\\"\r\n]|\\\\[nt\"\\\\])*\")");
  // Regex_new("^(\"[.\\]*\"$)");

  char match[MAX_TOKEN_LEN];
  int line_count = 1;

  while (*text != '\0')
    {
      /* match regular expressions */
      if (Regex_match (whitespace, text, match))
      {
        for (int i = 0; match[i] != '\0'; i++)
        {
            if (match[i] == '\n')
            {
              line_count++;
            }
        }
      }
      
      // STR
      else if (Regex_match(str_literals, text, match))
      {
        TokenQueue_add(tokens, Token_new(STRLIT, match, line_count));
      }
      
      // ID: also handle overlap of other regexs
      else if (Regex_match (identifiers, text, match))
      {
        /* TODO: implement line count and replace placeholder (-1) */
          if (Regex_match(keywords, text, match))
          {
            TokenQueue_add(tokens, Token_new(KEY, match, line_count));
          } else if (Regex_match(reserved, text, match))
          {
            Error_throw_printf ("Invalid use of reserved word: %s\n", text);
          }
          else 
          {
            TokenQueue_add(tokens, Token_new(ID, match, line_count));
          }
      }
      
      // DEC and HEX
      else if (Regex_match (decimals, text, match))
      {
        if (Regex_match(hex_literals, text, match))
        {
          TokenQueue_add (tokens, Token_new (HEXLIT, match, line_count));
        } else {
          TokenQueue_add (tokens, Token_new (DECLIT, match, line_count));
        }
      }

      // SYM
      else if (Regex_match(multi_symbols, text, match))
      {
        TokenQueue_add(tokens, Token_new(SYM, match, line_count));
      }
      else if (Regex_match (symbols, text, match))
      {
        TokenQueue_add (tokens, Token_new (SYM, match, line_count));
      }

      // handle overlap in regexs
      else if (Regex_match (letter, text, match))
      {
        TokenQueue_add(tokens, Token_new(ID, match, line_count));
      }

      else if (Regex_match (hex_literals, text, match))
      {
        TokenQueue_add (tokens, Token_new (HEXLIT, match, line_count));
      }

      else
      {
        Error_throw_printf ("Invalid token: %s\n", text);
      }

      /* skip matched text to look for next token */
      text += strlen (match);
    }

  line_count++;

  /* clean up */
  Regex_free (whitespace);
  Regex_free (letter);
  Regex_free (identifiers);
  Regex_free (decimals);
  Regex_free (symbols);
  Regex_free(multi_symbols);
  Regex_free (keywords);
  Regex_free (hex_literals);
  Regex_free(str_literals);
  Regex_free(reserved);

  return tokens;
}
