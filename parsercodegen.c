/*
Assignment:
HW3 - Parser and Code Generator for PL/0
Author(s): <Chen-An Chang>, <Luciano Paredes>
Language: C (only)
To Compile:
Scanner:
gcc -O2 -std=c11 -o lex lex.c
Parser/Code Generator:
gcc -O2 -std=c11 -o parsercodegen parsercodegen.c
To Execute (on Eustis):
./lex <input_file.txt>
./parsercodegen
where:
<input_file.txt> is the path to the PL/0 source program
Notes:
- lex.c accepts ONE command-line argument (input PL/0 source file)
- parsercodegen.c accepts NO command-line arguments
- Input filename is hard-coded in parsercodegen.c
- Implements recursive-descent parser for PL/0 grammar
- Generates PM/0 assembly code (see Appendix A for ISA)
- All development and testing performed on Eustis
Class: COP3402 - System Software - Fall 2025
Instructor: Dr. Jie Lin
Due Date: Friday, October 31, 2025 at 11:59 PM ET
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SYMBOL_TABLE_SIZE 500

// tokenstype enumeration
enum {
    skipsym = 1,
    identsym,
    numbersym,
    plussym,
    minussym,
    multsym,
    slashsym,
    eqsym,
    neqsym,
    lessym,
    leqsym,
    gtrsym,
    geqsym,
    lparentsym,
    rparentsym,
    commasym,
    semicolonsym,
    periodsym,
    becomessym,
    beginsym,
    endsym,
    ifsym,
    fisym,
    thensym,
    whilesym,
    dosym,
    callsym,
    constsym,
    varsym,
    procsym,
    writesym,
    readsym,
    elsesym,
    evensym
};

// PM/0 instruction set
enum {
    LIT = 1,
    OPR = 2,
    LOD = 3,
    STO = 4,
    CAL = 5,
    INC = 6,
    JMP = 7,
    JPC = 8,
    SYS = 9
};

// OPR codes
enum {
    OPR_RET = 0,
    OPR_ADD = 1,
    OPR_SUB = 2,
    OPR_MUL = 3,
    OPR_DIV = 4,
    OPR_EQL = 5,
    OPR_NEQ = 6,
    OPR_LSS = 7,
    OPR_LEQ = 8,
    OPR_GTR = 9,
    OPR_GEQ = 10,
    OPR_EVEN = 11
};

// symbol table
typedef struct {
    int kind;      // 1 = const, 2 = var, 3 = proc
    char name[12]; // up to 11 chars + '\0'
    int val;       // for constants
    int level;     // level (0)
    int addr;      // address for vars
    int mark;      // 0 = available, 1 = deleted/unavailable
} symbol;

symbol symbol_table[MAX_SYMBOL_TABLE_SIZE];
int sym_count = 0;

typedef struct {
    int op;
    int l;
    int m;
} instruction;

instruction code[500];
int code_ind = 0;

// token stream storage
int tokens[500];
char token_attr[500][64];
int tok_count = 0;
int cur = 0; // index of current token

const char *TOKEN_FILENAME = "tokens.txt";
const char *ELF_FILENAME = "elf.txt";

void emit(int op, int l, int m) {
    if (code_ind >= 500) {
        fprintf(stderr, "Internal error: code array overflow\n");
        exit(1);
    }
    code[code_ind].op = op;
    code[code_ind].l = l;
    code[code_ind].m = m;
    code_ind++;
}

int symbol_lookup(const char *name) {
    for (int i = 0; i < sym_count; i++) {
        if (symbol_table[i].mark == 0 && strcmp(symbol_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void symbol_insert_const(const char *name, int value) {
    if (symbol_lookup(name) != -1) {
        printf("Error: symbol name has already been declared\n");
        FILE *elf = fopen(ELF_FILENAME, "w");
        if (elf) {
            fprintf(elf, "Error: symbol name has already been declared\n");
            fclose(elf);
        }
        exit(1);
    }
    if (sym_count >= MAX_SYMBOL_TABLE_SIZE) {
        fprintf(stderr, "Symbol table overflow\n");
        exit(1);
    }
    symbol s = {1, "", value, 0, 0, 0};
    strncpy(s.name, name, 11);
    s.name[11] = 0;
    symbol_table[sym_count++] = s;
}

void symbol_insert_var(const char *name, int addr) {
    if (symbol_lookup(name) != -1) {
        printf("Error: symbol name has already been declared\n");
        FILE *elf = fopen(ELF_FILENAME, "w");
        if (elf) {
            fprintf(elf, "Error: symbol name has already been declared\n");
            fclose(elf);
        }
        exit(1);
    }
    if (sym_count >= MAX_SYMBOL_TABLE_SIZE) {
        fprintf(stderr, "Symbol table overflow\n");
        exit(1);
    }
    symbol s = {2, "", 0, 0, addr, 0};
    strncpy(s.name, name, 11);
    s.name[11] = 0;
    symbol_table[sym_count++] = s;
}
// token operations
int peek() {
    if (cur >= tok_count)
        return -1;
    return tokens[cur];
}
int advance() {
    if (cur >= tok_count)
        return -1;
    return tokens[cur++];
}
char* cur_attr() {
    if (cur == 0)
        return token_attr[0];
    if (cur - 1 >= 0 && cur - 1 < tok_count)
        return token_attr[cur - 1];
    return "";
}

void exit_msg(const char *msg) {
    printf("%s\n", msg);
    FILE *elf = fopen(ELF_FILENAME, "w");
    if (elf) {
        fprintf(elf, "%s\n", msg);
        fclose(elf);
    }
    exit(1);
}

// declare functions
void program();
void block();
int var_declaration();
void const_declaration();
void statement();
void condition();
void expression();
void term();
void factor();

void program() {
    block();
    if (peek() != periodsym) {
        exit_msg("Error: program must end with period");
    } else {
        advance(); // consume '.'
        for (int i = 0; i < sym_count; i++) {
            symbol_table[i].mark = 1;
        }
    }
    emit(SYS, 0, 3); // halt
}

void block() {
    const_declaration();
    int numVars = var_declaration();
    emit(INC, 0, 3 + numVars);
    statement();
}

void const_declaration() {
    if (peek() == constsym) {
        advance(); // consume 'const'
        while (1) {
            if (peek() != identsym) {
                exit_msg("Error: const, var, and read keywords must be followed by identifier");
            }
            advance(); // consume id
            // get name
            char name[12];
            strncpy(name, cur_attr(), 11);
            name[11] = 0;
            if (symbol_lookup(name) != -1) {
                exit_msg("Error: symbol name has already been declared");
            }
            if (peek() != eqsym) {
                exit_msg("Error: constants must be assigned with =");
            }
            advance(); // consume '='
            if (peek() != numbersym) {
                exit_msg("Error: constants must be assigned an integer value");
            }
            // capture number
            int val = atoi(token_attr[cur]);
            val = atoi(token_attr[cur]);
            advance(); // consume number
            symbol_insert_const(name, val);
            if (peek() == commasym) {
                advance();
                continue;
            } else break;
        }
        if (peek() != semicolonsym) {
            exit_msg("Error: constant and variable declarations must be followed by a semicolon");
        }
        advance(); // consume ';'
    }
}

int var_declaration() {
    int numVars = 0;
    if (peek() == varsym) {
        advance(); // consume 'var'
        while (1) {
            if (peek() != identsym) {
                exit_msg("Error: const, var, and read keywords must be followed by identifier");
            }
            // get name
            char name[12];
            strncpy(name, token_attr[cur], 11);
            name[11] = 0;
            advance(); // consume id
            if (symbol_lookup(name) != -1) {
                exit_msg("Error: symbol name has already been declared");
            }
            // addresses allocation per sample, first var addr = 3, second = 4, etc
            int addr = 3 + numVars;
            symbol_insert_var(name, addr);
            numVars++;
            if (peek() == commasym) {
                advance();
                continue;
            } else break;
        }
        if (peek() != semicolonsym) {
            exit_msg("Error: constant and variable declarations must be followed by a semicolon");
        }
        advance(); // consume ';'
    }
    return numVars;
}

void statement() {
    int tok = peek();
    if (tok == identsym) {
        char name[12];
        strncpy(name, token_attr[cur], 11);
        name[11] = 0;
        int idx = symbol_lookup(name);
        if (idx == -1) {
            exit_msg("Error: undeclared identifier");
        }
        if (symbol_table[idx].kind != 2) {
            exit_msg("Error: only variable values may be altered");
        }
        advance(); // consume id
        if (peek() != becomessym) {
            exit_msg("Error: assignment statements must use :=");
        }
        advance(); // consume ':='
        expression();
        emit(STO, 0, symbol_table[idx].addr);
        return;
    } else if (tok == beginsym) {
        advance(); // consume 'begin'
        statement();
        while (peek() == semicolonsym) {
            advance();
            statement();
        }
        if (peek() != endsym) {
            exit_msg("Error: begin must be followed by end");
        }
        advance(); // consume 'end'
        return;
    } else if (tok == ifsym) {
        advance();
        condition();
        int jpcIdx = code_ind;
        emit(JPC, 0, 0); // placeholder M
        if (peek() != thensym) {
            exit_msg("Error: if must be followed by then");
        }
        advance(); // consume 'then'
        statement();
        if (peek() != fisym) {
            exit_msg("Error: then must be followed by fi");
        }
        advance(); // consume 'fi'
        code[jpcIdx].m = code_ind;
        return;
    } else if (tok == whilesym) {
        advance();
        int loopIdx = code_ind;
        condition();
        if (peek() != dosym) {
            exit_msg("Error: while must be followed by do");
        }
        advance();
        int jpcIdx = code_ind;
        emit(JPC, 0, 0);
        statement();
        emit(JMP, 0, loopIdx);
        code[jpcIdx].m = code_ind;
        return;
    } else if (tok == readsym) {
        advance();
        if (peek() != identsym) {
            exit_msg("Error: const, var, and read keywords must be followed by identifier");
        }
        char name[12]; strncpy(name, token_attr[cur], 11); name[11]=0;
        int idx = symbol_lookup(name);
        if (idx == -1) {
            exit_msg("Error: undeclared identifier");
        }
        if (symbol_table[idx].kind != 2) {
            exit_msg("Error: only variable values may be altered");
        }
        advance(); // consume id
        emit(SYS, 0, 2);
        emit(STO, 0, symbol_table[idx].addr);
        return;
    } else if (tok == writesym) {
        advance();
        expression();
        emit(SYS, 0, 1); // write top of stack
        return;
    } else {
        // empty statement
        return;
    }
}

void condition() {
    if (peek() == evensym) {
        advance();
        expression();
        emit(OPR, 0, OPR_EVEN);
    } else {
        expression();
        int rel = peek();
        if (!(rel == eqsym || rel == neqsym || rel == lessym || rel == leqsym || rel == gtrsym || rel == geqsym)) {
            exit_msg("Error: condition must contain comparison operator");
        }
        advance(); // consume operator
        expression();
        // emit OPR
        if (rel == eqsym)
            emit(OPR, 0, OPR_EQL);
        else if (rel == neqsym)
            emit(OPR, 0, OPR_NEQ);
        else if (rel == lessym)
            emit(OPR, 0, OPR_LSS);
        else if (rel == leqsym)
            emit(OPR, 0, OPR_LEQ);
        else if (rel == gtrsym)
            emit(OPR, 0, OPR_GTR);
        else if (rel == geqsym)
            emit(OPR, 0, OPR_GEQ);
    }
}

void expression() {
    if (peek() == plussym || peek() == minussym) {
        int sign = peek();
        advance();
        if (sign == minussym) {
            emit(LIT, 0, 0);
            term();
            emit(OPR, 0, OPR_SUB);
        } else {
            term();
        }
    } else {
        term();
    }
    while (peek() == plussym || peek() == minussym) {
        int op = peek();
        advance();
        term();
        if (op == plussym) {
            emit(OPR, 0, OPR_ADD);
        } else {
            emit(OPR, 0, OPR_SUB);
        }
    }
}

void term() {
    factor();
    while (peek() == multsym || peek() == slashsym) {
        int op = peek();
        advance();
        factor();
        if (op == multsym) {
            emit(OPR, 0, OPR_MUL);
        } else {
            emit(OPR, 0, OPR_DIV);
        }
    }
}

void factor() {
    int t = peek();
    if (t == identsym) {
        char name[12];
        strncpy(name, token_attr[cur], 11);
        name[11] = 0;
        int idx = symbol_lookup(name);
        if (idx == -1) {
            exit_msg("Error: undeclared identifier");
        }
        if (symbol_table[idx].kind == 1) {
            emit(LIT, 0, symbol_table[idx].val);
        } else {
            emit(LOD, 0, symbol_table[idx].addr);
        }
        advance();
    } else if (t == numbersym) {
        int val = atoi(token_attr[cur]);
        emit(LIT, 0, val);
        advance();
    } else if (t == lparentsym) {
        advance();
        expression();
        if (peek() != rparentsym) exit_msg("Error: right parenthesis must follow left parenthesis");
        advance();
    } else {
        exit_msg("Error: arithmetic equations must contain operands, parentheses, numbers, or symbols");
    }
}

void read_token_file() {
    FILE *f = fopen(TOKEN_FILENAME, "r");
    if (!f) {
        fprintf(stderr, "Failed to open token file '%s'. If lex prints tokens to stdout, redirect: ./lex input.txt > tokens.txt\n", TOKEN_FILENAME);
        exit(1);
    }
    tok_count = 0;
    while (!feof(f) && tok_count < 500) {
        int tok;
        if (fscanf(f, "%d", &tok) != 1) {
            break;
        }
        tokens[tok_count] = tok;
        token_attr[tok_count][0] = '\0';
        if (tok == identsym || tok == numbersym) {
            char buf[128];
            if (fscanf(f, "%127s", buf) == 1) {
                strncpy(token_attr[tok_count], buf, 63);
                token_attr[tok_count][63] = 0;
            } else {
                token_attr[tok_count][0] = '\0';
            }
        }
        tok_count++;
    }
    fclose(f);
    // error check
    for (int i = 0; i < tok_count; i++) {
        if (tokens[i] == skipsym) {
            exit_msg("Error: Scanning error detected by lexer (skipsym present)");
        }
    }
}

void output() {
    printf("Assembly Code:\n\nLine OP L M\n\n");
    for (int i = 0; i < code_ind; i++) {
        const char *opname = "";
        switch (code[i].op) {
            case LIT:
                opname = "LIT";
                break;
            case OPR:
                opname = "OPR";
                break;
            case LOD:
                opname = "LOD";
                break;
            case STO:
                opname = "STO";
                break;
            case CAL:
                opname = "CAL";
                break;
            case INC:
                opname = "INC";
                break;
            case JMP:
                opname = "JMP";
                break;
            case JPC:
                opname = "JPC";
                break;
            case SYS:
                opname = "SYS";
                break;
            default:
                opname = "UNK";
                break;
        }
        printf("%d %s %d %d\n\n", i, opname, code[i].l, code[i].m);
    }
    printf("Symbol Table:\n\nKind | Name | Value | Level | Address | Mark\n\n");
    printf("---------------------------------------------------\n\n");
    for (int i = 0; i < sym_count; i++) {
        printf("%d | %s | %d | %d | %d | %d\n\n", symbol_table[i].kind, symbol_table[i].name, symbol_table[i].val, symbol_table[i].level, symbol_table[i].addr, symbol_table[i].mark);
    }

    // write numeric elf.txt file
    FILE *elf = fopen(ELF_FILENAME, "w");
    if (!elf) {
        fprintf(stderr, "Failed to open elf.txt for writing\n");
        exit(1);
    }
    for (int i = 0; i < code_ind; i++) {
        fprintf(elf, "%d %d %d\n", code[i].op, code[i].l, code[i].m);
    }
    fclose(elf);
}

int main() {
    // read token list from hard-coded filename
    read_token_file();

    // emit initial JMP 0 3 placeholder then code
    emit(JMP, 0, 3);

    // start parsing
    program();

    // after successful parse, output code and symbol table
    output();
    return 0;
}
