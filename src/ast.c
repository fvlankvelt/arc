#include "ast.h"

bool validate_expression(expr_t* expr)
{
    switch (expr->operation)
    {
    case FUNC:
        if (expr->type->kind != FUNC_KIND
            || expr->value.func.body->type != expr->type->arguments.function.return_type
            || !validate_expression(expr->value.func.body))
        {
            return false;
        }
        break;
    case EVAL:
        {
            const type_t* func_type = expr->value.eval.function->type;
            if (func_type->kind != FUNC_KIND
                || func_type->arguments.function.return_type != expr->type
                || func_type->arguments.function.argument_type != expr->value.eval.argument->type
                || !validate_expression(expr->value.eval.function)
                || !validate_expression(expr->value.eval.argument))
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
            || !validate_expression(expr->value.andor.left)
            || !validate_expression(expr->value.andor.right))
        {
            return false;
        }
        break;
    case NOT:
        if (expr->type->kind != BOOL_KIND
            || expr->value.not.negated->type->kind != BOOL_KIND
            || !validate_expression(expr->value.not.negated))
        {
            return false;
        }
        break;
    case IF:
        if (expr->value._if.predicate->type->kind != BOOL_KIND
            || expr->value._if._then->type != expr->type
            || expr->value._if._else->type != expr->type
            || !validate_expression(expr->value._if.predicate)
            || !validate_expression(expr->value._if._then)
            || !validate_expression(expr->value._if._else))
        {
            return false;
        }
        break;
    case EQ:
        if (expr->type->kind != BOOL_KIND
            || expr->value.eq.left->type != expr->value.eq.right->type
            || !validate_expression(expr->value.eq.left)
            || !validate_expression(expr->value.eq.right))
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
            || !validate_expression(expr->value.arithmetic.left)
            || !validate_expression(expr->value.arithmetic.right))
        {
            return false;
        }
        break;
    case CONS:
        if (expr->type->kind != LIST_KIND
            || expr->value.cons.head->type != expr->type->arguments.list.element_type
            || expr->value.cons.tail->type != expr->type)
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
            break;
        }
    case PAIR:
        if (expr->type->kind != PAIR_KIND
            || expr->value.pair.left->type != expr->type->arguments.pair.left_type
            || expr->value.pair.right->type != expr->type->arguments.pair.right_type)
        {
            return false;
        }
        break;
    case LEFT:
        if (expr->type != expr->value.leftright.pair->type->arguments.pair.left_type)
        {
            return false;
        }
        break;
    case RIGHT:
        if (expr->type != expr->value.leftright.pair->type->arguments.pair.right_type)
        {
            return false;
        }
        break;
    case EITHER:
        if (expr->type->kind != EITHER_KIND
            || (expr->value.either.is_left && expr->value.either.value->type != expr->type->arguments.
                either.left_type)
            || (!expr->value.either.is_left && expr->value.either.value->type != expr->type->arguments
                .either.right_type))
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
            break;
        }
    case PRIMITIVE:
    case REFERENCE:
        return true;
    default:
        return false;
    }
    return true;
}
