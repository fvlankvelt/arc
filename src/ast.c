#include "ast.h"

bool validate_expression(expr_t* expression)
{
    switch (expression->operation)
    {
    case FUNC:
        if (expression->type->kind != FUNC_KIND
            || expression->value.func.body->type != expression->type->arguments.function.return_type
            || !validate_expression(expression->value.func.body))
        {
            return false;
        }
        break;
    case EVAL:
        {
            const type_t* func_type = expression->value.eval.function->type;
            if (func_type->kind != FUNC_KIND
                || func_type->arguments.function.return_type != expression->type
                || func_type->arguments.function.argument_type != expression->value.eval.argument->type
                || !validate_expression(expression->value.eval.function)
                || !validate_expression(expression->value.eval.argument))
            {
                return false;
            }
            break;
        }
    case AND:
    case OR:
        if (expression->type->kind != BOOL_KIND
            || expression->value.andor.left->type->kind != BOOL_KIND
            || expression->value.andor.right->type->kind != BOOL_KIND
            || !validate_expression(expression->value.andor.left)
            || !validate_expression(expression->value.andor.right))
        {
            return false;
        }
        break;
    case NOT:
        if (expression->type->kind != BOOL_KIND
            || expression->value.not.negated->type->kind != BOOL_KIND
            || !validate_expression(expression->value.not.negated))
        {
            return false;
        }
        break;
    case IF:
        if (expression->value._if.predicate->type->kind != BOOL_KIND
            || expression->value._if._then->type != expression->type
            || expression->value._if._else->type != expression->type
            || !validate_expression(expression->value._if.predicate)
            || !validate_expression(expression->value._if._then)
            || !validate_expression(expression->value._if._else))
        {
            return false;
        }
        break;
    case EQ:
        if (expression->type->kind != BOOL_KIND
            || expression->value.eq.left->type != expression->value.eq.right->type
            || !validate_expression(expression->value.eq.left)
            || !validate_expression(expression->value.eq.right))
        {
            return false;
        }
        break;
    case ADD:
    case SUB:
    case MUL:
    case DIV:
        if (expression->type->kind != INT_KIND
            || expression->value.arithmetic.left->type->kind != INT_KIND
            || expression->value.arithmetic.right->type->kind != INT_KIND
            || !validate_expression(expression->value.arithmetic.left)
            || !validate_expression(expression->value.arithmetic.right))
        {
            return false;
        }
        break;
    case CONS:
        if (expression->type->kind != LIST_KIND
            || expression->value.cons.head->type != expression->type->arguments.list.element_type
            || expression->value.cons.tail->type != expression->type)
        {
            return false;
        }
        break;
    case ELEM:
        {
            type_t* result_type = expression->type;
            type_t* list_type = expression->value.elem.list->type;
            if (list_type->kind != LIST_KIND || expression->value.elem.empty->type != result_type)
            {
                return false;
            }
            type_t* element_type = list_type->arguments.list.element_type;
            type_t* nonempty_type = expression->value.elem.nonempty->type;
            if (nonempty_type->kind != FUNC_KIND
                || nonempty_type->arguments.function.argument_type != element_type)
            {
                return false;
            }
            type_t * curried_type = nonempty_type->arguments.function.return_type;
            if (curried_type->kind != FUNC_KIND
                || curried_type->arguments.function.argument_type != list_type
                || curried_type->arguments.function.return_type != result_type)
            {
                return false;
            }
            break;
        }
    case EITHER:
        if (expression->type->kind != EITHER_KIND
            || (expression->value.either.is_left && expression->value.either.value->type != expression->type->arguments.
                either.left_type)
            || (!expression->value.either.is_left && expression->value.either.value->type != expression->type->arguments
                .either.right_type))
        {
            return false;
        }
            break;
    case SWITCH:
        {
            type_t* either = expression->value._switch.either->type;
            type_t* left_type = expression->value._switch.map_left->type;
            type_t* right_type = expression->value._switch.map_right->type;
            if (left_type->kind != FUNC_KIND || right_type->kind != FUNC_KIND
                || left_type->arguments.function.return_type != expression->type
                || right_type->arguments.function.return_type != expression->type
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
