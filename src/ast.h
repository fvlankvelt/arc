#ifndef __AST_H__
#define __AST_H__
#include <math.h>

/**
 * Abstract syntax tree that should be easy to generate with transformer-based neural network.
 * The idea is to have a typed lambda calculus variant with some built-in common abstractions.
 *
 * Each function takes one argument, though it may return another function - effectively creating
 * one with two arguments.
 */

typedef enum _operation
{
    // the function <body> is an expression that can refer to the argument and to the function itself
    // (func <symbol> <body>)    [S] -> [T] -> [S -> T]
    FUNC,

    // (eval <func-expression> <expression>) [S -> T] -> [S] -> [T]
    EVAL,

    // built-in boolean functions
    // (and a b)    [Bool] -> [Bool] -> [Bool]
    AND,
    // (or a b)     [Bool] -> [Bool] -> [Bool]
    OR,
    // (not a)      [Bool] -> [Bool]
    NOT,
    // (if p a b)   [Bool] -> [T] -> [T] -> [T]
    IF,
    // (eq a b)     [T] -> [T] -> [Bool]
    EQ,

    // built-in integer functions
    // (+ a b)      [Int] -> [Int] -> [Int]
    ADD,
    // (- a b)      [Int] -> [Int] -> [Int]
    SUB,
    // (* a b)      [Int] -> [Int] -> [Int]
    MUL,
    // (/ a b)      [Int] -> [Int] -> [Int]
    DIV,

    // lists
    // (cons a b)   [T] -> [List[T]] -> [List[T]]
    CONS,
    // (elem a b c) [List[T]] -> [S] -> [T -> List[T] -> S] -> [S]
    ELEM,

    // tuples
    // (pair a b)   [T] -> [S] -> [Pair[T, S]]
    PAIR,
    // (left a)     [Pair[T, S]] -> [T]
    LEFT,
    // (right a)    [Pair[T, S]] -> [S]
    RIGHT,

    // union
    // (sum false t) [T] -> [Either[T, S]]
    // (sum true s)  [S] -> [Either[T, S]]
    EITHER,
    // (switch t s) [Either[T, S]] -> [T -> U] -> [S -> U] -> U
    SWITCH,

    // constants (no lambdas)
    // [Bool | Int | Nil | [Type]]
    PRIMITIVE,
    // a
    REFERENCE,
} operation_t;

/**
 * Parametrized types - in particular function, list & tuple need type arguments.
 */
typedef enum _kind
{
    BOOL_KIND, INT_KIND, TYPE_ARG_KIND, TYPE_REF_KIND, FUNC_KIND, LIST_KIND, PAIR_KIND, EITHER_KIND,
} kind_t;

/**
 * Instantiated types - all type arguments are present.
 */
typedef struct _type
{
    kind_t kind;

    union
    {
        struct
        {
            struct _type* element_type;
        } list;

        struct
        {
            struct _type* left_type;
            struct _type* right_type;
        } pair;

        struct
        {
            struct _type* left_type;
            struct _type* right_type;
        } either;

        struct
        {
            struct _type* return_type;
            struct _type* argument_type;
        } function;

        struct
        {
            int index;
        } reference;
    } arguments;
} type_t;

typedef struct _expr
{
    type_t* type;
    operation_t operation;
    const char * name;

    union
    {
        struct
        {
            // type of expression must match return type of function
            struct _expr* body;
        } func;

        struct
        {
            // argument type of function must match type of argument
            struct _expr* function; // must be a lambda
            struct _expr* argument;
        } eval;

        struct
        {
            // both must be bool expressions
            struct _expr* left;
            struct _expr* right;
        } andor;

        struct
        {
            // must be a bool expression
            struct _expr* negated;
        } not;

        struct
        {
            // must be a bool expression
            struct _expr* predicate;
            // these both must have same type
            struct _expr* _then;
            struct _expr* _else;
        } _if;

        struct
        {
            // these both must have same type
            struct _expr* left;
            struct _expr* right;
        } eq;

        struct
        {
            // both must be int expressions
            struct _expr* left;
            struct _expr* right;
        } arithmetic;

        struct
        {
            struct _expr* head;
            struct _expr* tail;
        } cons;

        struct
        {
            struct _expr* list;
            struct _expr* empty;
            struct _expr* nonempty;
        } elem;

        struct
        {
            struct _expr* left;
            struct _expr* right;
        } pair;

        struct
        {
            struct _expr* pair;
        } leftright;

        struct
        {
            bool is_left;
            struct _expr* value;
        } either;

        struct
        {
            struct _expr* either;
            struct _expr* map_left;
            struct _expr* map_right;
        } _switch;

        struct
        {
            unsigned int index;
        } reference;

        union
        {
            bool boolean;
            unsigned char integer;
            type_t* type;
        } primitive;
    } value;
} expr_t;

/**
 * (let <symbol> <expression value>
 *      <expression body>)
 *  assigns the value of <expression value> to <symbol> and
 *  returns <expression body> (which can refer to <symbol>)
 *
 * (lambda [<symbol>] <expression>)
 *  returns an anonymous function that can be evaluated with
 *  arguments that are bound to the list of symbols.
 *
 * (eval <symbol> [<expression>])
 *  evaluates a
 */
bool validate_expression(expr_t* expression);

#endif //__AST_H__
