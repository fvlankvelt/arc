#include "ast.h"

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
    type_t* a_resolved = a;
    if (a->kind == TYPE_REFERENCE_KIND && b->kind == TYPE_REFERENCE_KIND && a->arguments.reference.index == b->arguments
        .reference.index)
    {
        return true;
    }
    if (a->kind == TYPE_REFERENCE_KIND)
    {
        int index = a->arguments.reference.index;
        a_resolved = find_type(type_map, index);
        if (!a_resolved)
        {
            return false;
        }
    }
    type_t* b_resolved = b;
    if (b->kind == TYPE_REFERENCE_KIND)
    {
        int index = b->arguments.reference.index;
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
        if (!eq_types(a_resolved->arguments.function.return_type, b_resolved->arguments.function.return_type, type_map)
            || !eq_types(a_resolved->arguments.function.argument_type, b_resolved->arguments.function.argument_type,
                         type_map))
        {
            return false;
        }
        break;
    case LIST_KIND:
        if (!eq_types(a_resolved->arguments.list.element_type, b_resolved->arguments.list.element_type, type_map))
        {
            return false;
        }
        break;
    case PAIR_KIND:
        if (!eq_types(a_resolved->arguments.pair.left_type, b_resolved->arguments.pair.left_type, type_map)
            || !eq_types(a_resolved->arguments.pair.right_type, b_resolved->arguments.pair.right_type, type_map))
        {
            return false;
        }
        break;
    case EITHER_KIND:
        if (!eq_types(a_resolved->arguments.either.left_type, b_resolved->arguments.either.left_type, type_map)
            || !eq_types(a_resolved->arguments.either.right_type, b_resolved->arguments.either.right_type, type_map))
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
                || expr->value.func.body->type != expr->type->arguments.function.return_type)
            {
                return false;
            }
            arg_stack_t frame = {
                .type = expr->type->arguments.function.argument_type,
                .next = arg_stack,
            };
            if (!_validate(expr->value.func.body, &frame, type_map))
            {
                return false;
            }
            break;
        }
    case EVAL:
        {
            const type_t* func_type = expr->value.eval.function->type;
            if (func_type->kind != FUNC_KIND)
            {
                return false;
            }
            if (func_type->arguments.function.argument_type->kind == TYPE_KIND)
            {
                // when the argument of the function is a type, substitute the argument
                // that must be a primitive - there is no other way to reference types in an expression
                if (expr->value.eval.argument->type->kind != TYPE_REFERENCE_KIND
                    || expr->value.eval.argument->operation != PRIMITIVE)
                {
                    return false;
                }
                type_stack_t map_entry = {
                    .type = expr->value.eval.argument->value.primitive.type,
                    .next = type_map,
                };
                if (!eq_types(func_type->arguments.function.return_type, expr->type, &map_entry))
                {
                    return false;
                }
            }
            else
            {
                if (!eq_types(func_type->arguments.function.return_type, expr->type, type_map)
                    || !eq_types(func_type->arguments.function.argument_type, expr->value.eval.argument->type,
                                 type_map))
                {
                    return false;
                }
            }
            if (!_validate(expr->value.eval.function, arg_stack, type_map)
                || !_validate(expr->value.eval.argument, arg_stack, type_map))
            {
                return false;
            }
            break;
        }
    case AND:
    case OR:
        if (expr->type->kind != BOOL_KIND
            || expr->value.andor.left->type->kind != BOOL_KIND
            || expr->value.andor.right->type->kind != BOOL_KIND
            || !_validate(expr->value.andor.left, arg_stack, type_map)
            || !_validate(expr->value.andor.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case NOT:
        if (expr->type->kind != BOOL_KIND
            || expr->value.not.negated->type->kind != BOOL_KIND
            || !_validate(expr->value.not.negated, arg_stack, type_map))
        {
            return false;
        }
        break;
    case IF:
        if (expr->value._if.predicate->type->kind != BOOL_KIND
            || expr->value._if._then->type != expr->type
            || expr->value._if._else->type != expr->type
            || !_validate(expr->value._if.predicate, arg_stack, type_map)
            || !_validate(expr->value._if._then, arg_stack, type_map)
            || !_validate(expr->value._if._else, arg_stack, type_map))
        {
            return false;
        }
        break;
    case EQ:
        if (expr->type->kind != BOOL_KIND
            || expr->value.eq.left->type != expr->value.eq.right->type
            || !_validate(expr->value.eq.left, arg_stack, type_map)
            || !_validate(expr->value.eq.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case ADD:
    case SUB:
    case MUL:
    case DIV:
        if (expr->type->kind != INT_KIND
            || expr->value.arithmetic.left->type->kind != INT_KIND
            || expr->value.arithmetic.right->type->kind != INT_KIND
            || !_validate(expr->value.arithmetic.left, arg_stack, type_map)
            || !_validate(expr->value.arithmetic.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case CONS:
        if (expr->type->kind != LIST_KIND
            || expr->value.cons.head->type != expr->type->arguments.list.element_type
            || expr->value.cons.tail->type != expr->type
            || !_validate(expr->value.cons.head, arg_stack, type_map)
            || !_validate(expr->value.cons.tail, arg_stack, type_map))
        {
            return false;
        }
        break;
    case ELEM:
        {
            type_t* result_type = expr->type;
            type_t* list_type = expr->value.elem.list->type;
            if (list_type->kind != LIST_KIND || expr->value.elem.empty->type != result_type)
            {
                return false;
            }
            type_t* element_type = list_type->arguments.list.element_type;
            type_t* nonempty_type = expr->value.elem.nonempty->type;
            if (nonempty_type->kind != FUNC_KIND
                || nonempty_type->arguments.function.argument_type != element_type)
            {
                return false;
            }
            type_t* curried_type = nonempty_type->arguments.function.return_type;
            if (curried_type->kind != FUNC_KIND
                || curried_type->arguments.function.argument_type != list_type
                || curried_type->arguments.function.return_type != result_type)
            {
                return false;
            }
            if (!_validate(expr->value.elem.list, arg_stack, type_map)
                || !_validate(expr->value.elem.empty, arg_stack, type_map)
                || !_validate(expr->value.elem.nonempty, arg_stack, type_map))
            {
                return false;
            }
            break;
        }
    case PAIR:
        if (expr->type->kind != PAIR_KIND
            || expr->value.pair.left->type != expr->type->arguments.pair.left_type
            || expr->value.pair.right->type != expr->type->arguments.pair.right_type
            || !_validate(expr->value.pair.left, arg_stack, type_map)
            || !_validate(expr->value.pair.right, arg_stack, type_map))
        {
            return false;
        }
        break;
    case LEFT:
        if (expr->type != expr->value.leftright.pair->type->arguments.pair.left_type
            || !_validate(expr->value.leftright.pair, arg_stack, type_map))
        {
            return false;
        }
        break;
    case RIGHT:
        if (expr->type != expr->value.leftright.pair->type->arguments.pair.right_type
            || !_validate(expr->value.leftright.pair, arg_stack, type_map))
        {
            return false;
        }
        break;
    case EITHER:
        if (expr->type->kind != EITHER_KIND
            || (expr->value.either.is_left
                && expr->value.either.value->type != expr->type->arguments.either.left_type)
            || (!expr->value.either.is_left
                && expr->value.either.value->type != expr->type->arguments.either.right_type)
            || !_validate(expr->value.either.value, arg_stack, type_map))
        {
            return false;
        }
        break;
    case SWITCH:
        {
            type_t* either = expr->value._switch.either->type;
            type_t* left_type = expr->value._switch.map_left->type;
            type_t* right_type = expr->value._switch.map_right->type;
            if (left_type->kind != FUNC_KIND || right_type->kind != FUNC_KIND
                || left_type->arguments.function.return_type != expr->type
                || right_type->arguments.function.return_type != expr->type
                || left_type->arguments.function.argument_type != either->arguments.either.left_type
                || right_type->arguments.function.argument_type != either->arguments.either.right_type)
            {
                return false;
            }
            if (!_validate(expr->value._switch.either, arg_stack, type_map)
                || !_validate(expr->value._switch.map_left, arg_stack, type_map)
                || !_validate(expr->value._switch.map_right, arg_stack, type_map))
            {
                return false;
            }
            break;
        }
    case PRIMITIVE:
        return true;
    case REFERENCE:
        return arg_stack_has_index(arg_stack, expr->type, expr->value.reference.index);
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
struct
{
    type_t* type;
    expr_t* value;
} stack_frame_t;

expr_t* evaluate_expression(expr_t* expr)
{
}
