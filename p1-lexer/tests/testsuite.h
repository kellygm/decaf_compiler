/**
 * @file testsuite.h
 * @brief Testing utility functions
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include <check.h>

#include "p1-lexer.h"

/**
 * @brief Define a test case with text containing an invalid token
 */
#define TEST_INVALID(NAME,TEXT) START_TEST (NAME) \
{ ck_assert (invalid_tokens(TEXT)); } \
END_TEST

/**
 * @brief Define a test with a no tokens
 */
#define TEST_0TOKENS(NAME,TEXT) START_TEST (NAME) \
{ ck_assert (valid_0tokens(TEXT)); } \
END_TEST

/**
 * @brief Define a test with a single valid token
 */
#define TEST_1TOKEN(NAME,TEXT,ETYPE,ETEXT) START_TEST (NAME) \
{ ck_assert (valid_1token(TEXT, ETYPE, ETEXT)); } \
END_TEST

/**
 * @brief Define a test with two valid tokens
 */
#define TEST_2TOKENS(NAME,TEXT,ETYPE1,ETEXT1,ETYPE2,ETEXT2) START_TEST (NAME) \
{ ck_assert (valid_2tokens(TEXT, ETYPE1, ETEXT1, ETYPE2, ETEXT2)); } \
END_TEST

/**
 * @brief Define a text with an arbitrary number of tokens
 */
#define TEST_TOKENS(NAME,TEXT,NTOKENS,ETOKENS) START_TEST (NAME) \
{ ck_assert (valid_tokens(TEXT, NTOKENS, ETOKENS)); } \
END_TEST

/**
 * @brief Add a test to the test suite
 */
#define TEST(NAME) tcase_add_test (tc, NAME)

/**
 * @brief Run lexer on given text
 *
 * The difference between this and the main lex() function is that this version
 * catches exceptions and returns @c NULL instead of propogating the exception.
 *
 * @param text Code to lex
 * @returns Queue of tokens or @c NULL if there was a lexing error
 */
TokenQueue* run_lexer (char* text);

/**
 * @brief Run lexer on given text and check length of resulting token queue
 *
 * This function is identical to run_lexer() except that it also returns @c NULL
 * if the text was successfully lexed but the resulting queue is not of the
 * expected length.
 *
 * @param text Code to lex
 * @param expected_length Expected token queue length
 * @returns Queue of tokens or @c NULL if there was a lexing error or the
 * resulting queue's length doesn't match @c expected_length
 */
TokenQueue* run_lexer_check_size (char* text, size_t expected_length);

/**
 * @brief Run lexer on given text and verify that it throws an exception.
 *
 * @param text Code to lex
 * @returns True if and only if the lexer threw an exception
 */
bool invalid_tokens (char* text);

/**
 * @brief Run lexer on given text and verify that it returns no tokens (but also
 * does not throw an exception).
 *
 * @param text Code to lex
 * @returns True if and only if the text was lexed successfully as no tokens
 */
bool valid_0tokens (char* text);

/**
 * @brief Run lexer on given text and verify that it returns a single correct
 * token.
 *
 * @param text Code to lex
 * @param expected_type Expected type of the token
 * @param expected_text Expected text of the token
 * @returns True if and only if the token was lexed successfully as the expected
 * type
 */
bool valid_1token (char* text, TokenType expected_type, const char* expected_text);

/**
 * @brief Run lexer on given text and verify that it returns two correct tokens.
 *
 * @param text Code to lex
 * @param expected_type1 Expected type of the first token
 * @param expected_text1 Expected text of the first token
 * @param expected_type2 Expected type of the second token
 * @param expected_text2 Expected text of the second token
 * @returns True if and only if the tokens were lexed successfully as the
 * expected types
 */
bool valid_2tokens (char* text, TokenType expected_type1, const char* expected_text1,
                                TokenType expected_type2, const char* expected_text2);

/**
 * @brief Run lexer on given text and verify that it returns tokens of
 * the appropriate type.
 *
 * @param text Code to lex
 * @param ntokens Expected number of tokens
 * @param expected_tokens Array of expected token info (type and text)
 * @returns True if and only if the tokens were lexed successfully as the
 * expected types
 */
bool valid_tokens(char* text, size_t ntokens, Token expected_tokens[]);
