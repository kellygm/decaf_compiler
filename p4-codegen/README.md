
# P4: Code Generation
> Grade: A-
<br><br>
## Project Description:
The goal of this part of the project is to gain experience implementing code generation by converting Decaf ASTs into a linear code (ILOC). For this project, you will be implementing a NodeVisitor as discussed in class. This visitor should handle the conversion from an annotated AST (as an ASTNode) to sequential ILOC code (as an InsnList).
To do this conversion, you will write visitor methods that generate code for each AST node.
Implement code generation by providing an implementation for the following function:<br><br>
``` InsnList* generate_code (ASTNode* tree); ```
<br><br>
*For this particular project, you should pay special attention to the following files:*<br><br>
``` src/main.c ``` - The overall driver for the compiler. Right now it implements the lexer, parser, static analysis, and code generation phases of compilation. You may change this file if it is useful for testing, but your submission should work with the original version.
<br><br>
``` src/p4-codegen.c ``` - The code generation implementation. Your implementation of generate_code will go here, and you may add helper functions here if you find them useful (but note that they will be inaccessible to other modules--you should NOT modify p4-codegen.h).
<br><br>
``` include/iloc.h ``` - Describes the interface to ILOCInsn and its associated functions/methods, which you should use to write your solution. 
It also contains the visitor that allocates memory for all symbols, which runs after static analysis but before code generation (setting the "localSize" attribute, which you should use to emit the stack adjustment in function prologues). 
It contains some new helper functions for working with the "code" and "reg" attributes of ASTNode structures. Finally, it contains the ILOC simulator. YOU SHOULD NOT MODIFY THIS FILE. The implementations are in iloc.c, which you may want to skim as well (although you shouldn't change those either).
<br><br>
```tests``` - Testing framework folder. The student is responsible for extending the test suite to ensure solution completely implements this phase of the compiler as described by the language reference and description provided here.
