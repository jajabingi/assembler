
#  Two-Pass Assembler Project

##  Overview

This project implements a **two-pass assembler** in C.
The assembler translates a custom assembly-like language into machine code, producing output files with **object code**, **entries**, and **externals**.

The assembler handles:

* **Labels** (symbols, entries, externals).
* **Directives**: `.data`, `.string`, `.mat`, `.entry`, `.extern`.
* **Instructions**: `mov`, `add`, `sub`, `cmp`, `jmp`, `prn`, `inc`, `dec`, `lea`, `bne`, `stop`, etc.
* **Macros** (expand inline code with parameters).
* **Matrix addressing** (`M1[r2][r7]` style).
* **Validation** of labels, operands, and addressing modes.
* **Error detection** for undefined labels, invalid operands, multiple definitions, etc.

---

##  Project Structure

###
assembler/
├── assembler.h        # Global definitions, constants, data structures
├── main.c             # Entry point
├── first_pass.c/h     # Scans source, builds symbol table, relocates data
├── second_pass.c/h    # Resolves labels, generates final code
├── process_macros.c/h # Macro expansion
├── opmodes.c/h        # Addressing modes logic
├── words.c/h          # Encodes instructions/data into machine words
├── diag.c/h           # Error reporting & diagnostics
├── cleanup.c/h        # Free memory and cleanup helpers
├── prints.c           # Debug/printing helpers
├── makefile           # Build instructions
###

---

##  Build

Compile with:

###bash
make
###

Clean build artifacts:

###bash
make clean
###

This produces an executable:

###
./assembler <file1> <file2> ...
###

Each input file should have the extension **`.as`**.
The assembler automatically generates `.am`, `.ob`, `.ent`, `.ext` as needed.

---

##  Input Format

Example source (`ps.as`):

###asm
.entry LOOP
.entry LENGTH
.extern L3
.extern W

MAIN:   mov M1[r2][r7], W
        add r2, STR

LOOP:   jmp W
        prn #-5
        sub r1, r4
        inc K

        mov M1[r3][r3], r3
        bne L3

END:    stop

STR:    .string "abcdef"
LENGTH: .data 6, -9, 15
K:      .data 22
M1:     .mat [2][2] 1,2,3,4
###

---

##  Output Files

For each `file.as`, the assembler creates:

1. **`file.ob`**

   * Object file with addresses and encoded words (in **binary** or **base-4 letters**, depending on settings).

   Example:

   ###
   Addr: 100 | Bits10: 0000101100
   Addr: 101 | Bits10: 0000000000 | Label: K
   ...
   ###

   Or base-4 style:

   ###
   bcba aacda
   bcbb cbbac
   ###

2. **`file.ent`**

   * Contains addresses of `.entry` symbols.

   Example:

   ###
   LOOP    102
   LENGTH  106
   ###

3. **`file.ext`**

   * Contains addresses of `.extern` symbol usages.

   Example:

   ###
   W   100
   L3  115
   ###

---

##  Testing

You can test the assembler with the following provided input files:

* `ps.as` → basic example with `.string`, `.data`, `.mat`.
* `test101.as` 
* `test102.as` 
* `test104.as` 
* `course_input.as` 

Run:

###bash
./assembler ps test1 test2 test3 test_macro
###

Check the generated `.ob`, `.ent`, `.ext` files.

---

##  Error Handling

The assembler detects:

* Invalid label names.
* Undefined labels.
* Multiple label definitions.
* Invalid addressing modes.
* Too many resends (for file transfer logic).
* Missing `.extern` or `.entry` definitions.

Errors are reported with file name, line, and column info:

###
file.as:12:5: error: [AS001] invalid label name (must start with letter, max 31 chars)
###

---

##  Notes

* Addresses start at **100** (`IC_INIT_VALUE`).
* Each instruction or data is encoded into **10-bit words**.


