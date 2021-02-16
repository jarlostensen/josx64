# joforth
This project was inspired by this tweet by Petri Häkkinen @petrih3; https://twitter.com/petrih3/status/1357684425987936259 </br>
It made me realise that Forth is the ideal runtime for my kernel projects so I rolled up my sleeves and implemented a basic version that I can play with and use in my kernel.
</br>
It has no dependencies outside of standard C functionality, and it's built using CMake.
</br>
## It Is
The language features this interpreter implements are largely taken from this excellent Forth primer http://galileo.phys.virginia.edu/classes/551.jvn.fall01/primer.htm and is growing as I spend more time on it. 
</br>
Fundamentally the interpreter evaluates input statements (one at the time) and builds up the "VM" state, such as the dictionary of words. As is customary it contains a number of stacks for values and subroutine calls (words). </br>
Internally it maintains a dictionary of words which can be implemented as native C functions or as sequences of other words (in traditional Forth style) which are converted to an IR internally. 
This internal format provides both compactness and ease of implementing things like recursion and loops, in addition to making it easy to optimise particular functions. 

## Usage 

The joForth environment is created by providing an instance of the ```joforth_t``` structure which contains the VM state. 
```code c
    joforth._stack_size = 0;    
    joforth._rstack_size = 0;
    joforth._memory_size = 0;
    
    // joforth allocates all its memory through this interface
    joforth._allocator = *(&(joforth_allocator_t){
        ._alloc = malloc,
        ._free = free,
    });
    joforth_initialise(&joforth);
```

To define and invoke a word you invoke the interpreter with standard Forth statements:
```code c
joforth_eval(&joforth, ": GCD ( a b -- gcd)  ?DUP  IF  TUCK  MOD  recurse ENDIF ;");
joforth_eval(&joforth, "784 48 gcd dup .");
```

Executing the code snippet above produces the output ```16``` (which is the correct answer).

In ```main.c``` I have added some basic "unit tests" which provide more clues to how joForth works and what it can and can't (currently) do. </br>
Note that I am not using a testing framework as I deliberately didn't want to introduce external dependencies.

## It Is Not...
* Fast.
* ANS compliant.
* Clever.

That's all.


