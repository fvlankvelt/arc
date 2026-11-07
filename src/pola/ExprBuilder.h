#ifndef ARC_BUILDER_H
#define ARC_BUILDER_H

#include <vector>

#include "ast.h"
#include "util/mem.h"

typedef struct _type_stack {
    type_t* type;
    struct _type_stack* next;
} type_stack_t;

inline type_t* find_type(const type_stack_t* type_stack, int index) {
    if (!type_stack) {
        return nullptr;
    }
    if (!index) {
        return type_stack->type;
    }
    return find_type(type_stack->next, index - 1);
}

class ExprBuilder {
public:
    ExprBuilder() :
        _types(new_block(256, sizeof(type_t))),
        _exprs(new_block(256, sizeof(expr_t))) {
    }

    ~ExprBuilder() {
        free_block(this->_types);
        free_block(this->_exprs);
    }

    type_t* bool_type() {
        return &this->_bool_type;
    }

    type_t* int_type() {
        return &this->_int_type;
    }

    type_t* type_type() {
        return &this->_type_type;
    }

    type_t* list_type(type_t* element_type) {
        for (type_t* type : this->types) {
            if (type->kind == LIST_KIND && type->list.element_type == element_type) {
                return type;
            }
        }
        type_t* type = (type_t*)new_item(this->_types);
        type->kind = LIST_KIND;
        type->list.element_type = element_type;
        types.push_back(type);
        return type;
    }

    type_t* pair_type(type_t* left_type, type_t* right) {
        for (type_t* type : this->types) {
            if (type->kind == PAIR_KIND && type->pair.left_type == left_type && type->pair.right_type == right) {
                return type;
            }
        }
        type_t* type = (type_t*)new_item(this->_types);
        type->kind = PAIR_KIND;
        type->pair.left_type = left_type;
        type->pair.right_type = right;
        types.push_back(type);
        return type;
    }

    type_t* either_type(type_t* left_type, type_t* right) {
        for (type_t* type : this->types) {
            if (type->kind == EITHER_KIND && type->either.left_type == left_type && type->either.right_type == right) {
                return type;
            }
        }
        type_t* type = (type_t*)new_item(this->_types);
        type->kind = EITHER_KIND;
        type->either.left_type = left_type;
        type->either.right_type = right;
        types.push_back(type);
        return type;
    }

    type_t* func_type(type_t* return_type, type_t* argument_type) {
        for (type_t* type : this->types) {
            if (type->kind == FUNC_KIND && type->function.return_type == return_type && type->function.argument_type ==
                argument_type) {
                return type;
            }
        }
        type_t* type = (type_t*)new_item(this->_types);
        type->kind = FUNC_KIND;
        type->function.return_type = return_type;
        type->function.argument_type = argument_type;
        types.push_back(type);
        return type;
    }

    type_t* ref_type(int index) {
        for (type_t* type : this->types) {
            if (type->kind == TYPE_REFERENCE_KIND && type->reference.index == index) {
                return type;
            }
        }
        type_t* type = (type_t*)new_item(this->_types);
        type->kind = TYPE_REFERENCE_KIND;
        type->reference.index = index;
        types.push_back(type);
        return type;
    }

    expr_t* bool_primitive(bool value) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = bool_type();
        expr->operation = PRIMITIVE;
        expr->primitive.boolean = value;
        return expr;
    }

    expr_t* int_primitive(unsigned char value) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = int_type();
        expr->operation = PRIMITIVE;
        expr->primitive.integer = value;
        return expr;
    }

    expr_t* type_expr(type_t* value) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = type_type();
        expr->operation = PRIMITIVE;
        expr->primitive.type = value;
        return expr;
    }

    expr_t* func_expr(type_t* argument_type, expr_t* body) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = func_type(body->type, argument_type);
        expr->operation = FUNC;
        expr->func.body = body;
        return expr;
    }

    expr_t* eval_expr(expr_t* function, expr_t* argument) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = function->type->function.return_type;
        expr->operation = EVAL;
        expr->eval.function = function;
        expr->eval.argument = argument;
        return expr;
    }

    expr_t * andor_expr(operation_t operation, expr_t* left, expr_t * right) {
        assert(operation == AND || operation == OR);
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = bool_type();
        expr->operation = operation;
        expr->andor.left = left;
        expr->andor.right = right;
        return expr;
    }

    expr_t * not_expr(expr_t * negated) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = bool_type();
        expr->operation = NOT;
        expr->_not.negated = negated;
        return expr;
    }

    expr_t * int_expr(operation_t operation, expr_t * left, expr_t * right) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = int_type();
        expr->operation = operation;
        expr->arithmetic.left = left;
        expr->arithmetic.right = right;
        return expr;
    }

    expr_t* ref_expr(type_t* type, unsigned int index) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = type;
        expr->operation = REFERENCE;
        expr->reference.index = index;
        return expr;
    }

    expr_t* if_expr(expr_t* condition, expr_t* then_branch, expr_t* else_branch) {
        assert(condition->type == this->bool_type());
        assert(then_branch->type == else_branch->type);
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = then_branch->type;
        expr->operation = IF;
        expr->_if.predicate = condition;
        expr->_if._then = then_branch;
        expr->_if._else = else_branch;
        return expr;
    }

    expr_t* empty(type_t* element_type) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = list_type(element_type);
        expr->operation = PRIMITIVE;
        return expr;
    }

    expr_t* cons_expr(expr_t* head, expr_t* tail) {
        assert(tail->type->kind == LIST_KIND);
        assert(head->type == tail->type->list.element_type);
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = tail->type;
        expr->operation = CONS;
        expr->cons.head = head;
        expr->cons.tail = tail;
        return expr;
    }

    expr_t* elem_expr(expr_t* list, expr_t* empty, expr_t* nonempty) {
        assert(list->type->kind == LIST_KIND);
        assert(nonempty->type->kind == FUNC_KIND);
        assert(nonempty->type->function.argument_type == list->type->list.element_type);
        type_t* nonempty_return_type = nonempty->type->function.return_type;
        assert(nonempty_return_type->kind == FUNC_KIND);
        assert(nonempty_return_type->function.return_type == empty->type);
        assert(nonempty_return_type->function.argument_type == list->type);
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = empty->type;
        expr->operation = ELEM;
        expr->elem.list = list;
        expr->elem.empty = empty;
        expr->elem.nonempty = nonempty;
        return expr;
    }

    expr_t* pair_expr(expr_t* left, expr_t* right) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = pair_type(left->type, right->type);
        expr->operation = PAIR;
        expr->pair.left = left;
        expr->pair.right = right;
        return expr;
    }

    expr_t* leftright_expr(operation_t operation, expr_t * upstream) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        type_t * pair_type = upstream->type;
        expr->type = operation == LEFT ? pair_type->pair.left_type : pair_type->pair.right_type;
        expr->operation = operation;
        expr->leftright.pair = upstream;
        return expr;
    }

    expr_t* either_expr(expr_t* value, type_t* other_type, bool is_left) {
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = is_left ? either_type(value->type, other_type) : either_type(other_type, value->type);
        expr->operation = EITHER;
        expr->either.value = value;
        expr->either.is_left = is_left;
        return expr;
    }

    expr_t* switch_expr(expr_t* either, expr_t* map_left, expr_t* map_right) {
        assert(either->type->kind == EITHER_KIND);
        assert(map_left->type->kind == FUNC_KIND);
        assert(map_right->type->kind == FUNC_KIND);
        // assert(map_left->type->function.argument_type == either->type->either.left_type);
        // assert(map_right->type->function.argument_type == either->type->either.right_type);
        // assert(map_left->type->function.return_type == map_right->type->function.return_type);
        expr_t* expr = (expr_t*)new_item(this->_exprs);
        expr->type = map_left->type->function.return_type;
        expr->operation = SWITCH;
        expr->_switch.either = either;
        expr->_switch.map_left = map_left;
        expr->_switch.map_right = map_right;
        return expr;
    }

    type_t* resolve_type(type_t* type, type_stack_t* type_map) {
        if (type->kind == TYPE_REFERENCE_KIND) {
            type_t* resolved = find_type(type_map, type->reference.index);
            if (resolved) {
                return resolved;
            }
        }
        return type;
    }

    type_t * expand_type(type_t * type, type_stack_t * type_map) {
        switch (type->kind) {
        case FUNC_KIND:
            return func_type(
                expand_type(type->function.return_type, type_map),
                expand_type(type->function.argument_type, type_map)
            );
        case TYPE_REFERENCE_KIND:
            return resolve_type(type, type_map);
        case LIST_KIND:
            return list_type(expand_type(type->list.element_type, type_map));
        case PAIR_KIND:
            return pair_type(
                expand_type(type->pair.left_type, type_map),
                expand_type(type->pair.right_type, type_map)
            );
        case EITHER_KIND:
            return either_type(
                expand_type(type->either.left_type, type_map),
                expand_type(type->either.right_type, type_map)
            );
        default:
            return type;
        }
    }

    // expand type references
    // AST construction will be top-down, so the resolved type is known before the expression is constructed.
    void expand_types(expr_t* expr, type_stack_t* type_map) {
        switch (expr->operation) {
        case FUNC:
            expand_types(expr->func.body, type_map);
            break;
        case EVAL:
            {
                // push the type argument on the type stack
                type_t* func_type = expr->eval.function->type;
                if (func_type->function.argument_type->kind == TYPE_KIND
                    && expr->eval.argument->type->kind == TYPE_KIND
                    && expr->eval.argument->operation == PRIMITIVE) {
                    type_stack_t map_entry = {
                        .type = expr->eval.argument->primitive.type,
                        .next = type_map,
                    };
                    expr->type = expand_type(expr->type, &map_entry);
                    expand_types(expr->eval.function, &map_entry);
                    expand_types(expr->eval.argument, &map_entry);
                }
                else {
                    expand_types(expr->eval.function, type_map);
                    expand_types(expr->eval.argument, type_map);
                }
            }
            break;
        case PAIR:
            expand_types(expr->pair.left, type_map);
            expand_types(expr->pair.right, type_map);
            break;
        case LEFT:
        case RIGHT:
            expand_types(expr->leftright.pair, type_map);
            break;
        case EITHER:
            expand_types(expr->either.value, type_map);
            break;
        case SWITCH:
            expand_types(expr->_switch.either, type_map);
            expand_types(expr->_switch.map_left, type_map);
            expand_types(expr->_switch.map_right, type_map);
            break;
        case AND:
        case OR:
            expand_types(expr->andor.left, type_map);
            expand_types(expr->andor.right, type_map);
            break;
        case NOT:
            expand_types(expr->_not.negated, type_map);
            break;
        case IF:
            expand_types(expr->_if.predicate, type_map);
            expand_types(expr->_if._then, type_map);
            expand_types(expr->_if._else, type_map);
            break;
        case EQ:
        case ADD:
        case SUB:
        case MUL:
        case DIV:
            expand_types(expr->arithmetic.left, type_map);
            expand_types(expr->arithmetic.right, type_map);
            break;
        case CONS:
            expand_types(expr->cons.head, type_map);
            expand_types(expr->cons.tail, type_map);
            break;
        case ELEM:
            expand_types(expr->elem.list, type_map);
            expand_types(expr->elem.empty, type_map);
            expand_types(expr->elem.nonempty, type_map);
            break;
        case PRIMITIVE:
            break;
        case REFERENCE:
            break;
        }
    }

private:
    std::vector<type_t*> types;
    type_t _bool_type = {
        .kind = BOOL_KIND,
    };
    type_t _int_type = {
        .kind = INT_KIND,
    };
    type_t _type_type = {
        .kind = TYPE_KIND,
    };
    mem_block_t* _types;
    mem_block_t* _exprs;
};


#endif //ARC_BUILDER_H
