#include "testsuite.h"

jmp_buf decaf_error;

void Error_throw_printf (const char* format, ...)
{
    longjmp(decaf_error, 1);
}

ErrorList* run_analysis (char* text)
{
    ASTNode* tree = NULL;
    if (setjmp(decaf_error) == 0) {
        /* no error */
        tree = parse(lex(text));
    } else {
        /* error; return NULL */
        return NULL;
    }
    NodeVisitor_traverse_and_free(SetParentVisitor_new(), tree);
    NodeVisitor_traverse_and_free(CalcDepthVisitor_new(), tree);
    NodeVisitor_traverse_and_free(BuildSymbolTablesVisitor_new(), tree);
    return analyze(tree);
}

bool valid_program (char* text)
{
    ErrorList* errors = run_analysis(text);
    return errors != NULL && ErrorList_is_empty(errors);
}

bool invalid_program (char* text)
{
    ErrorList* errors = run_analysis(text);
    return errors == NULL || !ErrorList_is_empty(errors);
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
