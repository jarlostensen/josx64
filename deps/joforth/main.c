#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "joforth.h"

joforth_t joforth;

void test_incorrect_number(void) {
    assert(joforth_eval(&joforth, "0"));
    //assert(joforth_eval(&joforth, "0foobaar") == false);
    joforth_pop_value(&joforth);    
}

void test_dec_hex(void) {
    assert(joforth_eval(&joforth, "hex"));
    assert(joforth_eval(&joforth, "0x2f"));
    assert(joforth_top_value(&joforth)==0x2f);
    assert(joforth_eval(&joforth, "dec"));
    assert(joforth_eval(&joforth, "42"));
    assert(joforth_top_value(&joforth)==42);    
    assert(joforth_eval(&joforth, "popa"));
}

void test_define_word(void) {
    assert(joforth_eval(&joforth, ": squared ( a -- a*a ) dup *  ;"));
    assert(joforth_eval(&joforth, "80"));
    assert(joforth_eval(&joforth, "squared"));
    assert(joforth_top_value(&joforth)==6400);
    assert(joforth_eval(&joforth, "see squared"));
}

void test_recurse_statement(void) {
    assert(joforth_eval(&joforth, ": GCD    ( a b -- gcd)  ?DUP  IF  TUCK  MOD  recurse ENDIF ;"));
    assert(joforth_eval(&joforth, "784 48 gcd dup ."));
    assert(joforth_pop_value(&joforth) == 16);
    assert(joforth_eval(&joforth, "cr see gcd"));
}

void test_create_allot(void) {
    assert(joforth_eval(&joforth, "create X 8 cells allot"));
    assert(joforth_eval(&joforth, "X"));
    assert(joforth_eval(&joforth, "here"));
    joforth_value_t top1 = joforth_pop_value(&joforth);
    joforth_value_t top2 = joforth_pop_value(&joforth);
    assert(top1 == top2+64);
    // store 137 in the 5th cell, starting at X
    assert(joforth_eval(&joforth, "137  X 5 cells +  !"));
    // retrieve it
    assert(joforth_eval(&joforth, "X 5 cells + @"));
    assert(joforth_pop_value(&joforth) == 137);
}

void test_comparison(void) {
    assert(joforth_eval(&joforth, "2 3 >"));
    assert(joforth_pop_value(&joforth)==JOFORTH_FALSE);
    assert(joforth_eval(&joforth, "2 3 <"));
    assert(joforth_pop_value(&joforth)==JOFORTH_TRUE);
    assert(joforth_eval(&joforth, "3 3 ="));
    assert(joforth_pop_value(&joforth)==JOFORTH_TRUE);    
}

void test_ifthenelse(void) {
    assert(joforth_eval(&joforth, ".if-then-else 0 0 =  IF  TRUE  ELSE  .WRONG FALSE if .wronger endif ENDIF INVERT cr"));
    joforth_value_t tos = joforth_pop_value(&joforth);
    assert(tos == JOFORTH_FALSE);
    assert(joforth_eval(&joforth, ": TEST     0 =  INVERT  IF   CR   .\"Not zero!\"   ENDIF  ;"));
    assert(joforth_eval(&joforth, ".0 0 TEST cr"));
    assert(joforth_eval(&joforth, ".-14 -14 TEST cr"));
    assert(joforth_eval(&joforth, "see TEST cr"));
}

void test_arithmetic(void) {
    assert(joforth_eval(&joforth, "3 7 mod"));
    assert(joforth_pop_value(&joforth)==3);
    assert(joforth_eval(&joforth, "7 3 mod"));
    assert(joforth_pop_value(&joforth)==1);
}

void test_loops(void) {
    assert(joforth_eval(&joforth, ": COUNTDOWN    ( n --) BEGIN  CR   DUP  .  1 -   DUP   0  =   UNTIL  DROP  ;"));
    assert(joforth_eval(&joforth, "5 countdown cr"));
    assert(joforth_stack_is_empty(&joforth));
    assert(joforth_eval(&joforth, ".\"do-loop: \" 0 10 do .step... loop cr"));
}

void test_stack_ops(void) {
    assert(joforth_eval(&joforth, "2 drop"));
    assert(joforth_stack_is_empty(&joforth));
    assert(joforth_eval(&joforth, "73 -16 swap"));
    joforth_value_t tos = joforth_pop_value(&joforth);
    joforth_value_t nos = joforth_pop_value(&joforth);
    assert(tos == 73 && nos == -16);
    assert(joforth_eval(&joforth, "73 -16 tuck"));
    tos = joforth_pop_value(&joforth);
    nos = joforth_pop_value(&joforth);
    joforth_value_t nnos = joforth_pop_value(&joforth);
    assert(tos == -16 && nos == 73 && nnos == -16);
    assert(joforth_eval(&joforth, "0 ?dup"));
    tos = joforth_pop_value(&joforth);
    assert(tos==0);
    assert(joforth_stack_is_empty(&joforth));
    assert(joforth_eval(&joforth, "1 ?dup"));
    tos = joforth_pop_value(&joforth);
    assert(tos==1);
    assert(!joforth_stack_is_empty(&joforth));
    joforth_pop_value(&joforth);
}

int main(int argc, char* argv[]) {

    joforth._stack_size = 0;    
    joforth._memory_size = 0;
    joforth._allocator = *(&(joforth_allocator_t){
        ._alloc = malloc,
        ._free = free,
    });
    joforth_initialise(&joforth);

    assert(joforth_eval(&joforth, ".\"running tests..\" cr"));
    test_stack_ops();
    test_define_word();
    test_ifthenelse();
    test_dec_hex();
    test_loops();
    test_create_allot();
    test_incorrect_number();
    test_comparison();
    test_arithmetic();
    test_recurse_statement();
    
    printf(" bye\n");
    assert(joforth_stack_is_empty(&joforth));
    joforth_destroy(&joforth);
}
