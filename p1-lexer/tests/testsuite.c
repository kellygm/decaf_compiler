#include "testsuite.h"

jmp_buf decaf_error;

void Error_throw_printf (const char* format, ...)
{
    longjmp(decaf_error, 1);
}

TokenQueue* run_lexer (char* text)
{
    if (setjmp(decaf_error) == 0) {
        /* no error */
        return lex(text);
    } else {
        /* error; return NULL */
        return NULL;
    }
}

TokenQueue* run_lexer_check_size (char* text, size_t expected_length)
{
    TokenQueue* tokens = run_lexer(text);
    if (tokens == NULL) {
        /* lexing error */
        return NULL;
    }
    if (TokenQueue_size(tokens) != expected_length) {
        /* incorrect number of tokens */
        TokenQueue_free(tokens);
        return NULL;
    }
    return tokens;
}

bool invalid_tokens (char* text)
{
    return run_lexer(text) == NULL;
}

bool valid_0tokens (char* text)
{
    TokenQueue* tokens = run_lexer_check_size(text, 0);
    if (tokens == NULL)
        { return false; }
    TokenQueue_free(tokens);
    return true;
}

bool valid_1token (char* text, TokenType expected_type, const char* expected_text)
{
    TokenQueue* tokens = run_lexer_check_size(text, 1);
    if (tokens == NULL)
        { return false; }
    if (tokens->head->type != expected_type)            /* first token */
        { TokenQueue_free(tokens); return false; }
    if (strncmp(tokens->head->text, expected_text, MAX_TOKEN_LEN) != 0)
        { TokenQueue_free(tokens); return false; }
    TokenQueue_free(tokens);
    return true;
}

bool valid_2tokens (char* text, TokenType expected_type1, const char* expected_text1,
                                TokenType expected_type2, const char* expected_text2)
{
    TokenQueue* tokens = run_lexer_check_size(text, 2);
    if (tokens == NULL)
        { return false; }
    if (tokens->head->type != expected_type1)           /* first token */
        { TokenQueue_free(tokens); return false; }
    if (strncmp(tokens->head->text, expected_text1, MAX_TOKEN_LEN) != 0)
        { TokenQueue_free(tokens); return false; }
    if (tokens->head->next->type != expected_type2)     /* second token */
        { TokenQueue_free(tokens); return false; }
    if (strncmp(tokens->head->next->text, expected_text2, MAX_TOKEN_LEN) != 0)
        { TokenQueue_free(tokens); return false; }
    TokenQueue_free(tokens);
    return true;
}

bool valid_tokens(char* text, size_t ntokens, Token expected_tokens[])
{
    TokenQueue* tokens = run_lexer_check_size(text, ntokens);
    if (tokens == NULL)
        { return false; }
    for (size_t i = 0; i < ntokens; i++) {
        Token* token = TokenQueue_remove(tokens);
        if (token->type != expected_tokens[i].type)
            { TokenQueue_free(tokens); return false; }
        if (strncmp(token->text, expected_tokens[i].text, MAX_TOKEN_LEN) != 0)
            { TokenQueue_free(tokens); return false; }
        if (token->line != expected_tokens[i].line)
            { TokenQueue_free(tokens); return false; }
    }
    TokenQueue_free(tokens);
    return true;
}

extern void public_tests (Suite *s);
extern void private_tests (Suite *s);

Suite * test_suite (void)
{
    Suite *s = suite_create ("Default");
    public_tests (s);
    private_tests (s);
    return s;
}

void run_testsuite (void)
{
    Suite *s = test_suite ();
    SRunner *sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    srunner_free (sr);
}

int main (void)
{
    srand((unsigned)time(NULL));
    run_testsuite ();
    return EXIT_SUCCESS;
}
