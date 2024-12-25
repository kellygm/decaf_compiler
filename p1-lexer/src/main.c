/**
 * @file main.c
 * @brief Compiler driver
 */

#include "p1-lexer.h"

/**
 * @brief Error message buffer
 */
char decaf_error_msg[MAX_ERROR_LEN];

/**
 * @brief Data structure used by @c setjmp / @c longjmp for exception handling
 */
jmp_buf decaf_error;

/**
 * @brief Throw an exception with an error message using printf syntax
 *
 * This function is declared in common.h but must be defined here in main.c
 * because that's where the @c jmp_buf declaration is.
 */
void Error_throw_printf (const char* format, ...)
{
    /* delegate to vsnprintf for error message formatting */
    va_list args;
    va_start(args, format);
    vsnprintf(decaf_error_msg, MAX_ERROR_LEN, format, args);
    va_end(args);

    /* jump to location saved by setjmp */
    longjmp(decaf_error, 1);
}

/**
 * @brief Read all text data from a file
 *
 * @param filename Name of file to read
 * @param text String buffer destination (must be #MAX_FILE_SIZE characters long)
 * @returns True if and only if the file read was successful.
 */
bool read_file (const char* filename, char* text)
{
    FILE* input = fopen(filename, "r");
    if (input == NULL) {
        return false;
    }
    size_t nchars = 0;
    char* p = text;
    int c;
    while (nchars < MAX_FILE_SIZE && (c = fgetc(input)) != EOF) {
        *p++ = (char)c;
        nchars++;
    }
    *p = '\0';
    fclose(input);
    return true;
}

/**
 * @brief Compiler entry point
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @returns @c EXIT_SUCCESS if the compilation succeeds and @c EXIT_FAILURE
 * otherwise
 */
int main(int argc, char** argv)
{
    /* check for filename */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <decaf-filename>\n", argv[0]);
        return EXIT_FAILURE;
    }
    char* filename = argv[argc-1];

    /* read file */
    char text[MAX_FILE_SIZE];
    if (!read_file(filename, text)) {
        fprintf(stderr, "Could not read file: %s", filename);
        exit(EXIT_FAILURE);
    }

    /* FRONT END */

    TokenQueue* tokens = NULL;

    /* fatal errors are possible in the front end, so check for them */
    if (setjmp(decaf_error) == 0) {

        /* PROJECT 1: lexer */
        tokens = lex(text);

    } else {

        /* handle fatal error: print message and clean up */
        fprintf(stderr, "%s", decaf_error_msg);
        if (tokens != NULL) TokenQueue_free(tokens);
        exit(EXIT_FAILURE);
    }

    /* output */
    TokenQueue_print(tokens, stdout);

    /* clean up */
    TokenQueue_free(tokens);
    tokens = NULL;

    return EXIT_SUCCESS;
}
