# P5: Register Allocation
> Grade: A+

## Project Description
The goal of this part of the project is to implement a bottom-up register allocation algorithm, preparing ILOC for conversion to machine code. <br>
For this project, you will be implementing a routine that performs analysis on an InsnList that uses an arbitrary number of virtual registers, 
transforming it to only use a certain number of physical registers (presumably matching the number of physical registers available on the target architecture).
<br>
Implement code generation by providing an implementation for the following function:
<br><br>
```void allocate_registers (InsnList* list, int num_physical_registers);```
<br><br>

*For this particular project, you should pay special attention to the following files:* <br><br>
```src/main.c``` - The overall driver for the compiler. At this point it implements all phases of compilation. You may change this file if it is useful for testing, but your submission should work with the original version.
<br><br>
```src/p5-regalloc.c``` - The code generation implementation. Your implementation of allocate_registers will go here, and you may add helper functions here if you find them useful (but note that they will be inaccessible to other modules--you should NOT modify p5-regalloc.h).
<br><br>
```src/y86.c``` - Converts ILOC code into Y86 assembly. See the "Complete Compiler" section below for more details.
```tests``` - Testing framework folder.YOU WILL NOT BE PROVIDED ALL TEST CASES, only a few sanity test checks to help you estimate your progress in completing the project. You should significantly extend the provided test suite and convince yourself that your solution completely implements this phase of the compiler as described by the language reference and the description provided here.
<br><br>

Note that we only have a single register class that we need to worry about during this allocation (e.g., general-purpose integer registers), so there is no need to create a "Class" struct; just keep the data structures locally in p5-regalloc.c. 
I recommend just using arrays for most of the structures because they do not need to dynamically resize and the number of physical registers will be small. For register spilling, you will need to track the offsets of each spilled virtual register; I recommend using a map from virtual register ids to stack offsets.

