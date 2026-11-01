#include "ast.h"

#include "mem.h"

/**
 * Stack of lambda argument types, used when validating references
 */
typedef struct _arg_stack
{
    type_t* type;
    struct _arg_stack* next;
} arg_stack_t;

typedef struct _type_stack
{
    type_t* type;
    struct _type_stack* next;
} type_stack_t;

bool arg_stack_has_index(arg_stack_t* stack, type_t* type, unsigned int index)
{
    if (!stack)
    {
        return false;
    }
    if (stack->type == type)
    {
        if (!index)
        {
            return true;
        }
        return arg_stack_has_index(stack->next, type, index - 1);
    }
    return arg_stack_has_index(stack->next, type, index);
}

type_t* find_type(type_stack_t* type_map, int index)
{
    if (!type_map)
    {
        return nullptr;
    }
    if (!index)
    {
        return type_map->type;
    }
    return find_type(type_map->next, index - 1);
}

bool eq_types(type_t* a, type_t* b, type_stack_t* type_map)
{
    if (a->kind == TYPE_REFERENCE_KIND && b->kind == TYPE_REFERENCE_KIND
        && a->reference.index == b->reference.index)
    {
        return true;
    }
    type_t* a_resolved = a;
    if (a->kind == TYPE_REFERENCE_KIND)
    {
        int index = a->reference.index;
        a_resolved = find_type(type_map, index);
        if (!a_resolved)
        {
            return false;
        }
    }
    type_t* b_resolved = b;
    if (b->kind == TYPE_REFERENCE_KIND)
    {
        int index = b->reference.index;
        b_resolved = find_type(type_map, index);
        if (!b_resolved)
        {
            return false;
        }
    }
    if (a_resolved->kind != b_resolved->kind)
    {
        return false;
    }

    switch (a_resolved->kind)
    {
    case INT_KIND:
    case BOOL_KIND:
        break;
    case FUNC_KIND:
        if (!eq_types(a_resolved->function.return_type, b_resolved->function.return_type, type_map)
            || !eq_types(a_resolved->function.argument_type, b_resolved->function.argument_type,
                         type_map))
        {
            return false;
        }
        break;
    case LIST_KIND:
        if (!eq_types(a_resolved->list.element_type, b_resolved->list.element_type, type_map))
        {
            return false;
        }
        break;
    case PAIR_KIND:
        if (!eq_types(a_resolved->pair.left_type, b_resolved->pair.left_type, type_map)
            || !eq_types(a_resolved->pair.right_type, b_resolved->pair.right_type, type_map))
        {
            return false;
        }
        break;
    case EITHER_KIND:
        if (!eq_types(a_resolved->either.left_type, b_resolved->either.left_type, type_map)
            || !eq_types(a_resolved->either.right_type, b_resolved->either.right_type, type_map))
        {
            return false;
        }
        break;
    case TYPE_KIND:
        if (a != b)
        {
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

bool _validate(expr_t* expr, arg_stack_t* arg_stack, type_stack_t* type_map)
{
    switch (expr->operation)
    {
    case FUNC:
        {
            if (expr->type->kind != FUNC_KIND
                || expr->func.body->type != expr->type->function.return_type)
            {
                return false;
            }
            arg_stack_t frame = {
                .type = expr->type->function.argument_type,
                .next = arg_stack,
            };
            if (!_validate(expr->func.body, &frame, type_map))
            {
                return false;
            }
            break;
        }
    case EVAL:
        {
            const type_t* func_type = expr->eval.function->type;
            if (func_type->kind != FUNC_KIND)
            {
                return false;
            }
            if (func_type->function.argument_type->kind == TYPE_KIND)
            {
                // when the argument of the function is a type, substitute the argument
                // that must be a primitive - there is no other way to reference types in an expression
                if (expr->eval.argument->type->kind != TYPE_REFERENCE_KIND
                    || expr->eval.argument->operation != PRIMITIVE)
                {
                    return false;
                }
                type_stack_t map_entry = {
                    .type = expr->eval.argument->primitive.type,
                    .next = type_map,
                };
                if (!eq_types(func_type->function.return_type, expr->type, &map_entry))
                {
                    return false;
                }
            }
            else
            {
                if (!eq_types(func_type->function.return_type, expr->type, type_map)
                    || !eq_types(func_type->function.argument_type, expr->eval.argument->type,
                                 type_map))
                {
                    return false;
                }
            }
            if (!_validate(expr->eval.function, arg_stack, type_map)
                || !_validate(expr->eval.argument, arg_stack, type_map))
            {
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
            || !_validate(expr->andor.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case NOT:
        if (expr->type->kind != BOOL_KIND
            || expr->


               not.negated->type->kind != BOOL_KIND
            || !_validate(expr->not.negated, arg_stack, type_map))
        {
            return false;
        }
        break;
    case IF:
        if (expr->_if.predicate->type->kind != BOOL_KIND
            || expr->_if._then->type != expr->type
            || expr->_if._else->type != expr->type
            || !_validate(expr->_if.predicate, arg_stack, type_map)
            || !_validate(expr->_if._then, arg_stack, type_map)
            || !_validate(expr->_if._else, arg_stack, type_map))
        {
            return false;
        }
        break;
    case EQ:
        if (expr->type->kind != BOOL_KIND
            || expr->eq.left->type != expr->eq.right->type
            || !_validate(expr->eq.left, arg_stack, type_map)
            || !_validate(expr->eq.right, arg_stack, type_map))
        {
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
            || !_validate(expr->arithmetic.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case CONS:
        if (expr->type->kind != LIST_KIND
            || expr->cons.head->type != expr->type->list.element_type
            || expr->cons.tail->type != expr->type
            || !_validate(expr->cons.head, arg_stack, type_map)
            || !_validate(expr->cons.tail, arg_stack, type_map))
        {
            return false;
        }
        break;
    case ELEM:
        {
            type_t* result_type = expr->type;
            type_t* list_type = expr->elem.list->type;
            if (list_type->kind != LIST_KIND || expr->elem.empty->type != result_type)
            {
                return false;
            }
            type_t* element_type = list_type->list.element_type;
            type_t* nonempty_type = expr->elem.nonempty->type;
            if (nonempty_type->kind != FUNC_KIND
                || nonempty_type->function.argument_type != element_type)
            {
                return false;
            }
            type_t* curried_type = nonempty_type->function.return_type;
            if (curried_type->kind != FUNC_KIND
                || curried_type->function.argument_type != list_type
                || curried_type->function.return_type != result_type)
            {
                return false;
            }
            if (!_validate(expr->elem.list, arg_stack, type_map)
                || !_validate(expr->elem.empty, arg_stack, type_map)
                || !_validate(expr->elem.nonempty, arg_stack, type_map))
            {
                return false;
            }
            break;
        }
    case PAIR:
        if (expr->type->kind != PAIR_KIND
            || expr->pair.left->type != expr->type->pair.left_type
            || expr->pair.right->type != expr->type->pair.right_type
            || !_validate(expr->pair.left, arg_stack, type_map)
            || !_validate(expr->pair.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case LEFT:
        if (expr->type != expr->leftright.pair->type->pair.left_type
            || !_validate(expr->leftright.pair, arg_stack, type_map))
        {
            return false;
        }
        break;
    case RIGHT:
        if (expr->type != expr->leftright.pair->type->pair.right_type
            || !_validate(expr->leftright.pair, arg_stack, type_map))
        {
            return false;
        }
        break;
    case EITHER:
        if (expr->type->kind != EITHER_KIND
            || (expr->either.is_left
                && expr->either.value->type != expr->type->either.left_type)
            || (!expr->either.is_left
                && expr->either.value->type != expr->type->either.right_type)
            || !_validate(expr->either.value, arg_stack, type_map))
        {
            return false;
        }
        break;
    case SWITCH:
        {
            type_t* either = expr->_switch.either->type;
            type_t* left_type = expr->_switch.map_left->type;
            type_t* right_type = expr->_switch.map_right->type;
            if (left_type->kind != FUNC_KIND || right_type->kind != FUNC_KIND
                || left_type->function.return_type != expr->type
                || right_type->function.return_type != expr->type
                || left_type->function.argument_type != either->either.left_type
                || right_type->function.argument_type != either->either.right_type)
            {
                return false;
            }
            if (!_validate(expr->_switch.either, arg_stack, type_map)
                || !_validate(expr->_switch.map_left, arg_stack, type_map)
                || !_validate(expr->_switch.map_right, arg_stack, type_map))
            {
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

bool validate_expression(expr_t* expr)
{
    return _validate(expr, nullptr, nullptr);
}

/**
 * A lambda pushes its argument on the stack in a stack frame.
 * When
 */
typedef struct _eval_stack
{
    type_t* type;
    value_t* value;
    struct _eval_stack* next;
} eval_stack_t;

value_t* find_eval_stack(eval_stack_t* stack, type_t* type, unsigned int index, mem_block_t* block)
{
    if (!stack)
    {
        // shouldn't occur, right?
        value_t* result = new_item(block);
        *result = (value_t){
            .is_error = true,
            .error = {
                .message = "unresolved reference",
            }
        };
        return result;
    }
    if (stack->type == type)
    {
        if (!index)
        {
            return stack->value;
        }
        return find_eval_stack(stack->next, type, index - 1, block);
    }
    return find_eval_stack(stack->next, type, index, block);
}

bool eq_values(type_t* type, const value_t* left, const value_t* right)
{
    switch (type->kind)
    {
    case BOOL_KIND:
        return left->primitive.boolean == right->primitive.boolean;
    case INT_KIND:
        return left->primitive.integer == right->primitive.integer;
    case LIST_KIND:
        if (left->cons.head == nullptr || right->cons.head == nullptr)
        {
            return left->cons.head == right->cons.head;
        }
        return eq_values(type->list.element_type, left->cons.head, right->cons.head)
            && eq_values(type, left->cons.tail, right->cons.tail);
    case PAIR_KIND:
        return eq_values(type->pair.left_type, left->pair.left, right->pair.left)
            && eq_values(type->pair.right_type, left->pair.right, right->pair.right);
    case EITHER_KIND:
        if (left->either.is_left != right->either.is_left)
        {
            return false;
        }
        if (left->either.is_left)
        {
            return eq_values(type->either.left_type, left->either.value, right->either.value);
        }
        else
        {
            return eq_values(type->either.right_type, left->either.value, right->either.value);
        }
    }
    return false;
}

value_t* _evaluate(expr_t* expr, eval_stack_t* stack, mem_block_t* block)
{
    switch (expr->operation)
    {
    case FUNC:
        {
            value_t* result = new_item(block);
            result->is_error = false;
            result->function = expr;
            return result;
        }
    case EVAL:
        {
            value_t* function = _evaluate(expr->eval.function, stack, block);
            if (function->is_error)
            {
                return function;
            }
            value_t* argument = _evaluate(expr->eval.argument, stack, block);
            if (argument->is_error)
            {
                free_item(block, function);
                return argument;
            }
            eval_stack_t entry = {
                .type = expr->eval.argument->type,
                .value = argument,
                .next = stack,
            };
            value_t* result = _evaluate(function->function->func.body, &entry, block);
            free_item(block, function);
            free_item(block, argument);
            return result;
        }
        break;
    case AND:
    case OR:
        {
            value_t* left = _evaluate(expr->andor.left, stack, block);
            if (left->is_error)
            {
                return left;
            }
            value_t* right = _evaluate(expr->andor.right, stack, block);
            if (right->is_error)
            {
                free_item(block, left);
                return right;
            }
            value_t* result = new_item(block);
            result->is_error = false;
            switch (expr->operation)
            {
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
            value_t* upstream = _evaluate(expr->not.negated, stack, block);
            if (upstream->is_error)
            {
                return upstream;
            }
            upstream->primitive.boolean = !upstream->primitive.boolean;
            return upstream;
        }
    case IF:
        {
            value_t* predicate = _evaluate(expr->_if.predicate, stack, block);
            if (predicate->is_error)
            {
                return predicate;
            }
            bool condition = predicate->primitive.boolean;
            free_item(block, predicate);
            if (condition)
            {
                return _evaluate(expr->_if._then, stack, block);
            }
            else
            {
                return _evaluate(expr->_if._else, stack, block);
            }
        }
    case EQ:
        {
            value_t* left = _evaluate(expr->eq.left, stack, block);
            if (left->is_error)
            {
                return left;
            }
            value_t* right = _evaluate(expr->eq.right, stack, block);
            if (right->is_error)
            {
                free_item(block, left);
                return right;
            }
            value_t* result = new_item(block);
            result->is_error = false;
            result->primitive.boolean = eq_values(expr->eq.left->type, left, right);
            free_item(block, left);
            free_item(block, right);
            return result;
        }
        break;
    case ADD:
    case SUB:
    case MUL:
    case DIV:
        {
            value_t* left = _evaluate(expr->arithmetic.left, stack, block);
            if (left->is_error)
            {
                return left;
            }
            value_t* right = _evaluate(expr->arithmetic.right, stack, block);
            if (right->is_error)
            {
                free_item(block, left);
                return right;
            }
            value_t* result = new_item(block);
            result->is_error = false;
            switch (expr->operation)
            {
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
                if (right->primitive.integer == 0)
                {
                    result->is_error = true;
                    result->error.message = "division by zero";
                }
                else
                {
                    result->primitive.integer = left->primitive.integer / right->primitive.integer;
                }
                break;
            }
            return result;
        }
        break;
    case CONS:
        {
            value_t* head = _evaluate(expr->cons.head, stack, block);
            if (head->is_error)
            {
                return head;
            }
            value_t* tail = _evaluate(expr->cons.tail, stack, block);
            if (tail->is_error)
            {
                free_item(block, head);
                return tail;
            }
            value_t* result = new_item(block);
            result->is_error = false;
            result->cons.head = head;
            result->cons.tail = tail;
            return result;
        }
    case ELEM:
        {
            value_t* list = _evaluate(expr->elem.list, stack, block);
            if (list->is_error)
            {
                return list;
            }
            const bool empty = list->cons.head == nullptr;
            if (empty)
            {
                free_item(block, list);
                return _evaluate(expr->elem.empty, stack, block);
            }
            // resolve references, evaluate higher-order function to get the full function to apply to the head
            value_t* full_head_fn = _evaluate(expr->elem.nonempty, stack, block);
            if (full_head_fn->is_error)
            {
                free_item(block, list);
                return full_head_fn;
            }
            eval_stack_t head_entry = {
                .type = expr->elem.list->type->list.element_type,
                .value = list->cons.head,
                .next = stack,
            };
            value_t* tail_value = _evaluate(full_head_fn->function->func.body, &head_entry, block);
            free_item(block, full_head_fn);
            if (tail_value->is_error)
            {
                free_item(block, list);
                return tail_value;
            }
            value_t* full_tail_fn = _evaluate(tail_value->function, &head_entry, block);
            free_item(block, tail_value);
            if (full_tail_fn->is_error)
            {
                free_item(block, list);
                return full_tail_fn;
            }
            eval_stack_t tail_entry = {
                .type = expr->elem.list->type,
                .value = list->cons.tail,
                .next = &head_entry,
            };
            value_t* result = _evaluate(full_tail_fn->function->func.body, &tail_entry, block);
            free_item(block, list);
            free_item(block, full_tail_fn);
            return result;
        }
    case PAIR:
        {
            value_t* left = _evaluate(expr->pair.left, stack, block);
            if (left->is_error)
            {
                return left;
            }
            value_t* right = _evaluate(expr->pair.right, stack, block);
            if (right->is_error)
            {
                free_item(block, left);
                return right;
            }
            value_t* result = new_item(block);
            result->is_error = false;
            result->pair.left = left;
            result->pair.right = right;
            return result;
        }
    case LEFT:
        {
            value_t* value = _evaluate(expr->leftright.pair, stack, block);
            value_t* result = value->pair.left;
            value->pair.left = nullptr;
            free_item(block, value);
            return result;
        }
    case RIGHT:
        {
            value_t* value = _evaluate(expr->leftright.pair, stack, block);
            value_t* result = value->pair.right;
            value->pair.right = nullptr;
            free_item(block, value);
            return result;
        }
    case EITHER:
        {
            value_t* value = _evaluate(expr->either.value, stack, block);
            value_t* result = new_item(block);
            result->is_error = false;
            result->either.is_left = expr->either.is_left;
            result->either.value = value;
            return result;
        }
    case SWITCH:
        {
            value_t* either = _evaluate(expr->_switch.either, stack, block);
            bool is_left = either->either.is_left;
            value_t* fn;
            if (is_left)
            {
                fn = _evaluate(expr->_switch.map_left, stack, block);
            }
            else
            {
                fn = _evaluate(expr->_switch.map_right, stack, block);
            }
            eval_stack_t entry = {
                .type = expr->type,
                .value = either->either.value,
                .next = stack,
            };
            value_t* result = _evaluate(fn->function->func.body, &entry, block);
            return result;
        }
    case REFERENCE:
        return find_eval_stack(stack, expr->type, expr->reference.index, block);
    case PRIMITIVE:
        value_t* result = new_item(block);
        switch (expr->type->kind)
        {
        case BOOL_KIND:
            result->primitive.boolean = expr->primitive.boolean;
            break;
        case INT_KIND:
            result->primitive.integer = expr->primitive.integer;
            break;
        }
        return result;
    }
    value_t* result = new_item(block);
    *result = (value_t){
        .is_error = true,
        .error = {
            .message = "unhandled expression type",
        }
    };
    return result;
}

value_t* evaluate_expression(expr_t* expr, mem_block_t* block)
{
    return _evaluate(expr, nullptr, block);
}
