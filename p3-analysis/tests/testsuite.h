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
#include "p2-parser.h"
#include "p3-analysis.h"

/**
 * @brief Define a test case with a valid program
 */
#define TEST_VALID(NAME,TEXT) START_TEST (NAME) \
{ ck_assert (valid_program(TEXT)); } \
END_TEST

/**
 * @brief Define a test case with an invalid program
 */
#define TEST_INVALID(NAME,TEXT) START_TEST (NAME) \
{ ck_assert (invalid_program(TEXT)); } \
END_TEST

/**
 * @brief Define a test case with a valid main function
 */
#define TEST_VALID_MAIN(NAME,TEXT) START_TEST (NAME) \
{ ck_assert (valid_program("def int main () { " TEXT " }")); } \
END_TEST

/**
 * @brief Define a test case with an invalid main function
 */
#define TEST_INVALID_MAIN(NAME,TEXT) START_TEST (NAME) \
{ ck_assert (invalid_program("def int main () { " TEXT " }")); } \
END_TEST

/**
 * @brief Add a test to the test suite
 */
#define TEST(NAME) tcase_add_test (tc, NAME)

/**
 * @brief Run lexer, parser, and analysis on given text
 *
 * @param text Code to lex, parse, and analyze
 * @returns AST or @c NULL if there was an error
 */
ErrorList* run_analysis (char* text);

/**
 * @brief Run lexer and parser on given text and verify that it throws an exception.
 *
 * @param text Code to lex and parse
 * @returns True if and only if the lexer or parser threw an exception
 */
bool invalid_program (char* text);

/**
 * @brief Run lexer and parser on given text and verify that it returns an AST and
 * does not throw an exception.
 *
 * @param text Code to lex and parse
 * @returns True if and only if the text was lexed and parsed successfully
 */
bool valid_program (char* text);
