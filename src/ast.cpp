#include "ast.h"

#include "mem.h"

/**
 * Stack of lambda argument types, used when validating references
 */
typedef struct _arg_stack {
    type_t* type;
    struct _arg_stack* next;
} arg_stack_t;

typedef struct _type_stack {
    type_t* type;
    struct _type_stack* next;
} type_stack_t;

bool arg_stack_has_index(arg_stack_t* stack, type_t* type, unsigned int index) {
    if (!stack) {
        return false;
    }
    if (stack->type == type) {
        if (!index) {
            return true;
        }
        return arg_stack_has_index(stack->next, type, index - 1);
    }
    return arg_stack_has_index(stack->next, type, index);
}

type_t* find_type(const type_stack_t* type_stack, int index) {
    if (!type_stack) {
        return nullptr;
    }
    if (!index) {
        return type_stack->type;
    }
    return find_type(type_stack->next, index - 1);
}

bool eq_types(type_t* a, type_t* b, type_stack_t* type_map) {
    if (a->kind == TYPE_REFERENCE_KIND && b->kind == TYPE_REFERENCE_KIND
        && a->reference.index == b->reference.index) {
        return true;
    }
    type_t* a_resolved = a;
    if (a->kind == TYPE_REFERENCE_KIND) {
        int index = a->reference.index;
        a_resolved = find_type(type_map, index);
        if (!a_resolved) {
            return false;
        }
    }
    type_t* b_resolved = b;
    if (b->kind == TYPE_REFERENCE_KIND) {
        int index = b->reference.index;
        b_resolved = find_type(type_map, index);
        if (!b_resolved) {
            return false;
        }
    }
    if (a_resolved->kind != b_resolved->kind) {
        return false;
    }

    switch (a_resolved->kind) {
    case INT_KIND:
    case BOOL_KIND:
        break;
    case FUNC_KIND:
        if (!eq_types(a_resolved->function.return_type, b_resolved->function.return_type, type_map)
            || !eq_types(a_resolved->function.argument_type, b_resolved->function.argument_type,
                         type_map)) {
            return false;
        }
        break;
    case LIST_KIND:
        if (!eq_types(a_resolved->list.element_type, b_resolved->list.element_type, type_map)) {
            return false;
        }
        break;
    case PAIR_KIND:
        if (!eq_types(a_resolved->pair.left_type, b_resolved->pair.left_type, type_map)
            || !eq_types(a_resolved->pair.right_type, b_resolved->pair.right_type, type_map)) {
            return false;
        }
        break;
    case EITHER_KIND:
        if (!eq_types(a_resolved->either.left_type, b_resolved->either.left_type, type_map)
            || !eq_types(a_resolved->either.right_type, b_resolved->either.right_type, type_map)) {
            return false;
        }
        break;
    case TYPE_KIND:
        if (a != b) {
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

bool _validate(expr_t* expr, arg_stack_t* arg_stack, type_stack_t* type_map) {
    switch (expr->operation) {
    case FUNC:
        {
            if (expr->type->kind != FUNC_KIND
                || !eq_types(expr->func.body->type, expr->type->function.return_type, type_map)) {
                return false;
            }
            arg_stack_t frame = {
                .type = expr->type->function.argument_type,
                .next = arg_stack,
            };
            if (!_validate(expr->func.body, &frame, type_map)) {
                return false;
            }
            break;
        }
    case EVAL:
        {
            const type_t* func_type = expr->eval.function->type;
            if (func_type->kind != FUNC_KIND) {
                return false;
            }
            if (func_type->function.argument_type->kind == TYPE_KIND) {
                // when the argument of the function is a type, substitute the argument
                // that must be a primitive - there is no other way to reference types in an expression
                if (expr->eval.argument->type->kind != TYPE_KIND
                    || expr->eval.argument->operation != PRIMITIVE) {
                    return false;
                }
                type_stack_t map_entry = {
                    .type = expr->eval.argument->primitive.type,
                    .next = type_map,
                };
                if (!eq_types(func_type->function.return_type, expr->type, &map_entry)) {
                    return false;
                }
            }
            else {
                if (!eq_types(func_type->function.return_type, expr->type, type_map)
                    || !eq_types(func_type->function.argument_type, expr->eval.argument->type,
                                 type_map)) {
                    return false;
                }
            }
            if (!_validate(expr->eval.function, arg_stack, type_map)
                || !_validate(expr->eval.argument, arg_stack, type_map)) {
                return false;
            }
            break;
        }
    case AND:
    case OR:
        if (expr->type->kind != BOOL_KIND
            || expr->andor.left->type->kind != BOOL_KIND
            || expr->andor.right->type->kind != BOOL_KIND
            || !_validate(expr->andor.left, arg_stack, type_map)
            || !_validate(expr->andor.right, arg_stack, type_map)) {
            return false;
        }
        break;
    case NOT:
        if (expr->type->kind != BOOL_KIND
            || expr->_not.negated->type->kind != BOOL_KIND
            || !_validate(expr->_not.negated, arg_stack, type_map)) {
            return false;
        }
        break;
    case IF:
        if (expr->_if.predicate->type->kind != BOOL_KIND
            || !eq_types(expr->_if._then->type, expr->type, type_map)
            || !eq_types(expr->_if._else->type, expr->type, type_map)
            || !_validate(expr->_if.predicate, arg_stack, type_map)
            || !_validate(expr->_if._then, arg_stack, type_map)
            || !_validate(expr->_if._else, arg_stack, type_map)) {
            return false;
        }
        break;
    case EQ:
        if (expr->type->kind != BOOL_KIND
            || !eq_types(expr->eq.left->type, expr->eq.right->type, type_map)
            || !_validate(expr->eq.left, arg_stack, type_map)
            || !_validate(expr->eq.right, arg_stack, type_map)) {
            return false;
        }
        break;
    case ADD:
    case SUB:
    case MUL:
    case DIV:
        if (expr->type->kind != INT_KIND
            || expr->arithmetic.left->type->kind != INT_KIND
            || expr->arithmetic.right->type->kind != INT_KIND
            || !_validate(expr->arithmetic.left, arg_stack, type_map)
            || !_validate(expr->arithmetic.right, arg_stack, type_map)) {
            return false;
        }
        break;
    case CONS:
        if (expr->type->kind != LIST_KIND
            || !eq_types(expr->cons.head->type, expr->type->list.element_type, type_map)
            || !eq_types(expr->cons.tail->type, expr->type, type_map)
            || !_validate(expr->cons.head, arg_stack, type_map)
            || !_validate(expr->cons.tail, arg_stack, type_map)) {
            return false;
        }
        break;
    case ELEM:
        {
            type_t* result_type = expr->type;
            type_t* list_type = expr->elem.list->type;
            if (list_type->kind != LIST_KIND || !eq_types(expr->elem.empty->type, result_type, type_map)) {
                return false;
            }
            type_t* element_type = list_type->list.element_type;
            type_t* nonempty_type = expr->elem.nonempty->type;
            if (nonempty_type->kind != FUNC_KIND
                || !eq_types(nonempty_type->function.argument_type, element_type, type_map)) {
                return false;
            }
            type_t* curried_type = nonempty_type->function.return_type;
            if (curried_type->kind != FUNC_KIND
                || !eq_types(curried_type->function.argument_type, list_type, type_map)
                || !eq_types(curried_type->function.return_type, result_type, type_map)) {
                return false;
            }
            if (!_validate(expr->elem.list, arg_stack, type_map)
                || !_validate(expr->elem.empty, arg_stack, type_map)
                || !_validate(expr->elem.nonempty, arg_stack, type_map)) {
                return false;
            }
            break;
        }
    case PAIR:
        if (expr->type->kind != PAIR_KIND
            || !eq_types(expr->pair.left->type, expr->type->pair.left_type, type_map)
            || !eq_types(expr->pair.right->type, expr->type->pair.right_type, type_map)
            || !_validate(expr->pair.left, arg_stack, type_map)
            || !_validate(expr->pair.right, arg_stack, type_map)) {
            return false;
        }
        break;
    case LEFT:
        if (!eq_types(expr->type, expr->leftright.pair->type->pair.left_type, type_map)
            || !_validate(expr->leftright.pair, arg_stack, type_map)) {
            return false;
        }
        break;
    case RIGHT:
        if (!eq_types(expr->type, expr->leftright.pair->type->pair.right_type, type_map)
            || !_validate(expr->leftright.pair, arg_stack, type_map)) {
            return false;
        }
        break;
    case EITHER:
        if (expr->type->kind != EITHER_KIND
            || (expr->either.is_left
                && !eq_types(expr->either.value->type, expr->type->either.left_type, type_map))
            || (!expr->either.is_left
                && !eq_types(expr->either.value->type, expr->type->either.right_type, type_map))
            || !_validate(expr->either.value, arg_stack, type_map)) {
            return false;
        }
        break;
    case SWITCH:
        {
            type_t* either = expr->_switch.either->type;
            type_t* left_type = expr->_switch.map_left->type;
            type_t* right_type = expr->_switch.map_right->type;
            if (left_type->kind != FUNC_KIND || right_type->kind != FUNC_KIND
                || !eq_types(left_type->function.return_type, expr->type, type_map)
                || !eq_types(right_type->function.return_type, expr->type, type_map)
                || !eq_types(left_type->function.argument_type, either->either.left_type, type_map)
                || !eq_types(right_type->function.argument_type, either->either.right_type, type_map)) {
                return false;
            }
            if (!_validate(expr->_switch.either, arg_stack, type_map)
                || !_validate(expr->_switch.map_left, arg_stack, type_map)
                || !_validate(expr->_switch.map_right, arg_stack, type_map)) {
                return false;
            }
            break;
        }
    case PRIMITIVE:
        return true;
    case REFERENCE:
        return arg_stack_has_index(arg_stack, expr->type, expr->reference.index);
    default:
        return false;
    }
    return true;
}

bool validate_expression(expr_t* expr) {
    return _validate(expr, nullptr, nullptr);
}

class Arena {
public:
    Arena(mem_block_t* value_block)
        : value_block(value_block),
          stack_block(new_block(256, sizeof(stack_t))) {
    }

    value_t* new_value() {
        value_t* value = static_cast<value_t*>(new_item(value_block));
        *value = (value_t){
            .is_error = false,
            .ref_count = 1
        };
        return value;
    }

    stack_t* new_stack(type_t* type, value_t* value, stack_t* next) {
        stack_t* stack = static_cast<stack_t*>(new_item(stack_block));
        *stack = (stack_t){
            .ref_count = 1,
            .type = type,
            .value = value,
            .next = next
        };
        if (next) {
            next->ref_count++;
        }
        return stack;
    }

    void free_value(const type_t* type, value_t* value) {
        if (!value) {
            return;
        }
        value->ref_count--;
        if (value->ref_count > 0) {
            return;
        }
        if (!value->is_error) {
            switch (type->kind) {
            case BOOL_KIND:
            case INT_KIND:
                break;
            case FUNC_KIND:
                free_stack(value->closure.stack);
                break;
            case LIST_KIND:
                free_value(type->list.element_type, value->cons.head);
                free_value(type, value->cons.tail);
                break;
            case PAIR_KIND:
                free_value(type->pair.left_type, value->pair.left);
                free_value(type->pair.right_type, value->pair.right);
                break;
            case EITHER_KIND:
                value->either.is_left
                    ? free_value(type->either.left_type, value->either.value)
                    : free_value(type->either.right_type, value->either.value);
                break;
            }
        }
        free_item(value_block, value);
    }

    void free_stack(stack_t* stack) {
        if (!stack) {
            return;
        }
        stack->ref_count--;
        if (stack->ref_count > 0) {
            return;
        }
        free_stack(stack->next);
        free_value(stack->type, stack->value);
        free_item(stack_block, stack);
    }

private:
    mem_block_t* value_block;
    mem_block_t* stack_block;
};

// helper for memory management of values.
class Value {
public:
    Value(type_t* type, Arena& arena) : type(type), arena(arena) {
        value = arena.new_value();
        *value = (value_t){
            .is_error = false,
            .ref_count = 1
        };
    }

    Value(type_t* type, value_t* value, Arena& arena) :
        type(type), arena(arena), value(value) {
        value->ref_count++;
    }

    Value(const Value& other) :
        type(other.type), arena(other.arena), value(other.value) {
        value->ref_count++;
    }

    ~Value() {
        arena.free_value(type, value);
    }

    type_t* get_type() const {
        return type;
    }

    value_t* get() {
        value->ref_count++;
        return value;
    }

    void set(value_t* new_value) {
        arena.free_value(type, value);
        value = new_value;
        value->ref_count++;
    }

    value_t* unsafe() {
        return value;
    }

    value_t* operator->() {
        return value;
    }

private:
    type_t* type;
    value_t* value;
    Arena& arena;
};

class Stack {
public:
    Stack(stack_t* stack, Arena& arena) : stack(stack), arena(arena) {
    }

    ~Stack() {
        arena.free_stack(stack);
    }

    stack_t* unsafe() {
        return stack;
    }

    stack_t* operator->() {
        return stack;
    }

private:
    stack_t* stack;
    Arena& arena;
};

/**
 * A lambda pushes its argument on the stack in a stack frame.
 */
Value find_eval_stack(stack_t* stack, type_t* type, unsigned int index, Arena& arena) {
    if (!stack) {
        // shouldn't occur, right?
        Value result(type, arena);
        result->is_error = true;
        result->error.message = "unresolved reference";
        return result;
    }
    if (stack->type == type) {
        if (!index) {
            return {stack->type, stack->value, arena};
        }
        return find_eval_stack(stack->next, type, index - 1, arena);
    }
    return find_eval_stack(stack->next, type, index, arena);
}

inline type_t* resolve_type(type_t* type, stack_t* stack) {
    if (type->kind == TYPE_REFERENCE_KIND) {
        int index = type->reference.index;
        while (stack) {
            if (stack->type->kind == TYPE_KIND) {
                if (!index) {
                    return stack->value->primitive.type;
                }
                index--;
            }
            stack = stack->next;
        }
    }
    return type;
}

class Evaluator {
public:
    Evaluator(mem_block_t* block) : arena(Arena(block)) {
    }

    Value evaluate(const expr_t* expr, stack_t* stack);

private:
    Arena arena;
};

bool eq_values(const type_t* type, const value_t* left, const value_t* right) {
    switch (type->kind) {
    case BOOL_KIND:
        return left->primitive.boolean == right->primitive.boolean;
    case INT_KIND:
        return left->primitive.integer == right->primitive.integer;
    case LIST_KIND:
        if (left->cons.head == nullptr || right->cons.head == nullptr) {
            return left->cons.head == right->cons.head;
        }
        return eq_values(type->list.element_type, left->cons.head, right->cons.head)
            && eq_values(type, left->cons.tail, right->cons.tail);
    case PAIR_KIND:
        return eq_values(type->pair.left_type, left->pair.left, right->pair.left)
            && eq_values(type->pair.right_type, left->pair.right, right->pair.right);
    case EITHER_KIND:
        if (left->either.is_left != right->either.is_left) {
            return false;
        }
        if (left->either.is_left) {
            return eq_values(type->either.left_type, left->either.value, right->either.value);
        }
        else {
            return eq_values(type->either.right_type, left->either.value, right->either.value);
        }
    }
    return false;
}

Value Evaluator::evaluate(const expr_t* expr, stack_t* stack) {
    Value result(resolve_type(expr->type, stack), arena);
    switch (expr->operation) {
    case FUNC:
        {
            result->closure.function = expr;
            result->closure.stack = stack;
            if (stack) {
                stack->ref_count++;
            }
            return result;
        }
    case EVAL:
        {
            Value function(evaluate(expr->eval.function, stack));
            if (function->is_error) {
                return function;
            }
            Value argument(evaluate(expr->eval.argument, stack));
            if (argument->is_error) {
                return argument;
            }
            Stack entry(
                arena.new_stack(
                    argument.get_type(),
                    argument.get(),
                    function->closure.stack
                ), arena);
            return evaluate(function->closure.function->func.body, entry.unsafe());
        }
        break;
    case AND:
    case OR:
        {
            Value left(evaluate(expr->andor.left, stack));
            if (left->is_error) {
                return left;
            }
            Value right(evaluate(expr->andor.right, stack));
            if (right->is_error) {
                return right;
            }
            switch (expr->operation) {
            case AND:
                result->primitive.boolean = left->primitive.boolean && right->primitive.boolean;
                break;
            case OR:
                result->primitive.boolean = left->primitive.boolean || right->primitive.boolean;
                break;
            }
            return result;
        }
    case NOT:
        {
            Value upstream(evaluate(expr->_not.negated, stack));
            if (upstream->is_error) {
                return upstream;
            }
            upstream->primitive.boolean = !upstream->primitive.boolean;
            return upstream;
        }
    case IF:
        {
            Value predicate(evaluate(expr->_if.predicate, stack));
            if (predicate->is_error) {
                return predicate;
            }
            if (predicate->primitive.boolean) {
                return evaluate(expr->_if._then, stack);
            }
            else {
                return evaluate(expr->_if._else, stack);
            }
        }
    case EQ:
        {
            Value left(evaluate(expr->eq.left, stack));
            if (left->is_error) {
                return left;
            }
            Value right(evaluate(expr->eq.right, stack));
            if (right->is_error) {
                return right;
            }
            result->primitive.boolean = eq_values(expr->eq.left->type, left.unsafe(), right.unsafe());
            return result;
        }
        break;
    case ADD:
    case SUB:
    case MUL:
    case DIV:
        {
            Value left(evaluate(expr->arithmetic.left, stack));
            if (left->is_error) {
                return left;
            }
            Value right(evaluate(expr->arithmetic.right, stack));
            if (right->is_error) {
                return right;
            }
            switch (expr->operation) {
            case ADD:
                result->primitive.integer = left->primitive.integer + right->primitive.integer;
                break;
            case SUB:
                result->primitive.integer = left->primitive.integer - right->primitive.integer;
                break;
            case MUL:
                result->primitive.integer = left->primitive.integer * right->primitive.integer;
                break;
            case DIV:
                if (right->primitive.integer == 0) {
                    result->is_error = true;
                    result->error.message = "division by zero";
                }
                else {
                    result->primitive.integer = left->primitive.integer / right->primitive.integer;
                }
                break;
            }
            return result;
        }
        break;
    case CONS:
        {
            Value head(evaluate(expr->cons.head, stack));
            if (head->is_error) {
                return head;
            }
            Value tail(evaluate(expr->cons.tail, stack));
            if (tail->is_error) {
                return tail;
            }
            result->cons.head = head.get();
            result->cons.tail = tail.get();
            return result;
        }
    case ELEM:
        {
            Value list(evaluate(expr->elem.list, stack));
            if (list->is_error) {
                return list;
            }
            if (list->cons.head == nullptr) {
                return evaluate(expr->elem.empty, stack);
            }
            // resolve references, evaluate higher-order function to get the full function to apply to the head
            Value head_fn(evaluate(expr->elem.nonempty, stack));
            if (head_fn->is_error) {
                return head_fn;
            }
            type_t* element_type = expr->elem.list->type->list.element_type;
            Value head(element_type, list->cons.head, arena);
            Stack head_entry(
                arena.new_stack(
                    element_type,
                    head.get(),
                    head_fn->closure.stack
                ), arena);
            Value tail_fn_ctor(evaluate(head_fn->closure.function->func.body, head_entry.unsafe()));
            if (tail_fn_ctor->is_error) {
                return tail_fn_ctor;
            }
            Value tail_fn(evaluate(tail_fn_ctor->closure.function, head_entry.unsafe()));
            if (tail_fn->is_error) {
                return tail_fn;
            }
            Value tail(expr->elem.list->type, list->cons.tail, arena);
            Stack tail_entry(
                arena.new_stack(
                    expr->elem.list->type,
                    tail.get(),
                    head_entry.unsafe()
                ), arena);
            return evaluate(tail_fn->closure.function->func.body, tail_entry.unsafe());
        }
    case PAIR:
        {
            Value left(evaluate(expr->pair.left, stack));
            if (left->is_error) {
                return left;
            }
            Value right(evaluate(expr->pair.right, stack));
            if (right->is_error) {
                return right;
            }
            result->pair.left = left.get();
            result->pair.right = right.get();
            return result;
        }
    case LEFT:
        {
            Value value(evaluate(expr->leftright.pair, stack));
            result.set(value->pair.left);
            return result;
        }
    case RIGHT:
        {
            Value value(evaluate(expr->leftright.pair, stack));
            result.set(value->pair.right);
            return result;
        }
    case EITHER:
        {
            Value value(evaluate(expr->either.value, stack));
            result->either.is_left = expr->either.is_left;
            result->either.value = value.get();
            return result;
        }
    case SWITCH:
        {
            Value either(evaluate(expr->_switch.either, stack));
            bool is_left = either->either.is_left;
            if (is_left) {
                type_t* left_type = expr->_switch.either->type->either.left_type;
                Value value(left_type, either->either.value, arena);
                Value fn(evaluate(expr->_switch.map_left, stack));
                Stack entry(
                    arena.new_stack(
                        left_type,
                        value.get(),
                        fn->closure.stack
                    ), arena);
                return evaluate(fn->closure.function->func.body, entry.unsafe());
            }
            else {
                type_t* right_type = expr->_switch.either->type->either.right_type;
                Value value(right_type, either->either.value, arena);
                Value fn(evaluate(expr->_switch.map_right, stack));
                Stack entry(
                    arena.new_stack(
                        right_type,
                        value.get(),
                        fn->closure.stack
                    ), arena);
                return evaluate(fn->closure.function->func.body, entry.unsafe());
            }
        }
    case REFERENCE:
        return find_eval_stack(stack, resolve_type(expr->type, stack), expr->reference.index, arena);
    case PRIMITIVE:
        switch (expr->type->kind) {
        case BOOL_KIND:
            result->primitive.boolean = expr->primitive.boolean;
            break;
        case INT_KIND:
            result->primitive.integer = expr->primitive.integer;
            break;
        case TYPE_KIND:
            result->primitive.type = expr->primitive.type;
            break;
        }
        return result;
    }
    result->is_error = true;
    result->error.message = "unhandled expression type";
    return result;
}

value_t* evaluate_expression(expr_t* expr, mem_block_t* block) {
    Evaluator evaluator(block);
    Value result(evaluator.evaluate(expr, nullptr));
    return result.get();
}
