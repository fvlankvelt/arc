#ifndef ARC_PARTIALEXPR_H
#define ARC_PARTIALEXPR_H

#include "ExprBuilder.h"

class PartialExpr {
public:
    PartialExpr(ExprBuilder& builder, type_t* type) :
        builder(builder), type(type) {
    };
    virtual ~PartialExpr();

    virtual const operation_t operation() const;

    virtual std::vector<type_t*> slots() const;

    virtual void put(int idx, expr_t* expr);

    virtual expr_t* build() const;

protected:
    ExprBuilder& builder;
    type_t* type;
};

class FuncExpr : PartialExpr {
public:
    FuncExpr(ExprBuilder& builder, type_t* func_type) : PartialExpr(
        builder, func_type) {
        assert(func_type->kind == FUNC_KIND);
        body = nullptr;
    }

    const operation_t operation() const override {
        return FUNC;
    }

    std::vector<type_t*> slots() const override {
        return {type->function.return_type};
    }

    void put(int idx, expr_t* expr) override {
        assert(idx == 0);
        assert(expr->type == type->function.return_type);
        body = expr;
    }

    expr_t* build() const override {
        return builder.func_expr(
            type->function.argument_type,
            body
        );
    }

private:
    expr_t* body;
};

class EvalExpr : PartialExpr {
public:
    EvalExpr(ExprBuilder& builder, type_t* result_type, type_t* argument_type) :
        PartialExpr(builder, result_type),
        argument_type(argument_type) {
        func_type = builder.func_type(type, argument_type);
        function = nullptr;
        argument = nullptr;
    }

    const operation_t operation() const override {
        return EVAL;
    }

    std::vector<type_t*> slots() const override {
        return {
            func_type,
            argument_type,
        };
    }

    void put(int idx, expr_t* expr) override {
        switch (idx) {
        case 0:
            assert(expr->type == func_type);
            function = expr;
            break;
        case 1:
            assert(expr->type == argument_type);
            argument = expr;
            break;
        default:
            assert(false);
        }
    }

    expr_t* build() const override {
        return builder.eval_expr(
            function,
            argument
        );
    }

private:
    type_t* func_type;
    type_t* argument_type;

    expr_t* function;
    expr_t* argument;
};

class BoolExpr : PartialExpr {
public:
    BoolExpr(ExprBuilder& builder, operation_t operation) :
        PartialExpr(builder, builder.bool_type()), _operation(operation) {
        assert(operation == AND || operation == OR || operation == NOT);
        first = nullptr;
        second = nullptr;
    }

    const operation_t operation() const override {
        return _operation;
    }

    std::vector<type_t*> slots() const override {
        switch (_operation) {
        case NOT:
            return {builder.bool_type()};
        default:
            return {builder.bool_type(), builder.bool_type()};
        }
    }

    void put(int idx, expr_t* expr) override {
        assert((_operation == NOT && idx < 1) || idx < 2);
        assert(expr->type == builder.bool_type());
        if (idx == 0) {
            first = expr;
        }
        else {
            second = expr;
        }
    }

    expr_t* build() const override {
        switch (_operation) {
        case NOT:
            return builder.not_expr(first);
        default:
            return builder.andor_expr(_operation, first, second);
        }
    }

private:
    operation_t _operation;
    expr_t* first;
    expr_t* second;
};

class IfExpr : PartialExpr {
public:
    IfExpr(ExprBuilder& builder, type_t* type) : PartialExpr(builder, type) {
        predicate = nullptr;
        _then = nullptr;
        _else = nullptr;
    }

    const operation_t operation() const override {
        return IF;
    }

    std::vector<type_t*> slots() const override {
        return {
            builder.bool_type(),
            type,
            type,
        };
    }

    void put(int idx, expr_t* expr) override {
        switch (idx) {
        case 0:
            assert(expr->type == builder.bool_type());
            predicate = expr;
            break;
        case 1:
            assert(expr->type == type);
            _then = expr;
            break;
        case 2:
            assert(expr->type == type);
            _else = expr;
            break;
        default:
            assert(false);
        }
    }

    expr_t* build() const override;

private:
    expr_t* predicate;
    expr_t* _then;
    expr_t* _else;
};

class EqExpr : PartialExpr {
public:
    EqExpr(ExprBuilder& builder, type_t* arg_type) :
        PartialExpr(builder, builder.bool_type()), arg_type(arg_type) {
        left = nullptr;
        right = nullptr;
    }

    const operation_t operation() const override {
        return EQ;
    }

    std::vector<type_t*> slots() const override {
        return {arg_type, arg_type};
    }

    void put(int idx, expr_t* expr) override {
        assert(idx < 2);
        assert(expr->type == arg_type);
        if (!idx) {
            left = expr;
        }
        else {
            right = expr;
        }
    }

    expr_t* build() const override;

private:
    type_t* arg_type;
    expr_t* left;
    expr_t* right;
};

class ArithExpr : PartialExpr {
public:
    ArithExpr(ExprBuilder& builder, operation_t operation) :
        PartialExpr(builder, builder.int_type()),
        _operation(operation) {
        assert(operation == ADD || operation == SUB || operation == MUL || operation == DIV);
        first = nullptr;
        second = nullptr;
    }

    const operation_t operation() const override {
        return _operation;
    }

    std::vector<type_t*> slots() const override {
        return {builder.int_type(), builder.int_type()};
    }

    void put(int idx, expr_t* expr) override {
        assert(idx < 2);
        assert(expr->type == builder.int_type());
        if (!idx) {
            first = expr;
        }
        else {
            second = expr;
        }
    }

    expr_t* build() const override {
        return builder.int_expr(_operation, first, second);
    }

private:
    operation_t _operation;
    expr_t* first;
    expr_t* second;
};

class ConsExpr : PartialExpr {
public:
    ConsExpr(ExprBuilder& builder, type_t* element_type) :
        PartialExpr(builder, builder.list_type(element_type)),
        element_type(element_type) {
        head = nullptr;
        tail = nullptr;
    }

    const operation_t operation() const override {
        return CONS;
    }

    std::vector<type_t*> slots() const override {
        return {element_type, type};
    }

    void put(int idx, expr_t* expr) override {
        switch (idx) {
        case 0:
            assert(expr->type == element_type);
            head = expr;
            break;
        case 1:
            assert(expr->type == type);
            tail = expr;
            break;
        default:
            assert(false);
        }
    }

    expr_t* build() const override {
        return builder.cons_expr(head, tail);
    }

private:
    type_t* element_type;
    expr_t* head;
    expr_t* tail;
};

class ElemExpr : PartialExpr {
public:
    ElemExpr(ExprBuilder& builder, type_t* type, type_t* element_type) :
        PartialExpr(builder, type), element_type(element_type) {
        nonempty_type = builder.func_type(
            builder.func_type(
                type,
                builder.list_type(element_type)
            ),
            element_type
        );
        list = nullptr;
        empty = nullptr;
        nonempty = nullptr;
    }

    const operation_t operation() const override {
        return ELEM;
    }

    std::vector<type_t*> slots() const override {
        return {
            builder.list_type(element_type),
            type,
            nonempty_type
        };
    }

    void put(int idx, expr_t* expr) override {
        switch (idx) {
        case 0:
            assert(expr->type == builder.list_type(element_type));
            list = expr;
            break;
        case 1:
            assert(expr->type == type);
            empty = expr;
            break;
        case 2:
            assert(expr->type == nonempty_type);
            nonempty = expr;
            break;
        default:
            assert(false);
        }
    }

    expr_t* build() const override {
        return builder.elem_expr(list, empty, nonempty);
    }

private:
    type_t* element_type;
    type_t* nonempty_type;
    expr_t* list;
    expr_t* empty;
    expr_t* nonempty;
};

class PairExpr : PartialExpr {
public:
    PairExpr(ExprBuilder& builder, type_t* left_type, type_t* right_type) :
        PartialExpr(builder, builder.pair_type(left_type, right_type)) {
        left = nullptr;
        right = nullptr;
    }

    const operation_t operation() const override {
        return PAIR;
    }

    std::vector<type_t*> slots() const override {
        return {type->pair.left_type, type->pair.right_type};
    }

    void put(int idx, expr_t* expr) override {
        switch (idx) {
        case 0:
            assert(expr->type == type->pair.left_type);
            left = expr;
            break;
        case 1:
            assert(expr->type == type->pair.right_type);
            right = expr;
            break;
        default:
            assert(false);
        }
    }

    expr_t* build() const override {
        return builder.pair_expr(left, right);
    }

private:
    expr_t* left;
    expr_t* right;
};

class LeftRightExpr : PartialExpr {
public:
    LeftRightExpr(ExprBuilder& builder, operation_t operation, type_t* pair_type) :
        PartialExpr(builder, operation == LEFT ? pair_type->pair.left_type : pair_type->pair.right_type),
        pair_type(pair_type),
        _operation(operation) {
        assert(operation == LEFT || operation == RIGHT);
        upstream = nullptr;
    }

    const operation_t operation() const override {
        return _operation;
    }

    std::vector<type_t*> slots() const override {
        return {pair_type};
    }

    void put(int idx, expr_t* expr) override {
        assert(idx == 0);
        assert(expr->type == pair_type);
        upstream = expr;
    }

    expr_t* build() const override {
        return builder.leftright_expr(_operation, upstream);
    }

private:
    type_t* pair_type;
    operation_t _operation;
    expr_t* upstream;
};

class EitherExpr : PartialExpr {
public:
    EitherExpr(ExprBuilder& builder, type_t* left_type, type_t* right_type, bool is_left) :
        PartialExpr(builder, builder.either_type(left_type, right_type)),
        is_left(is_left) {
        value = nullptr;
    }

    const operation_t operation() const override {
        return EITHER;
    }

    std::vector<type_t*> slots() const override {
        return {is_left ? type->either.left_type : type->either.right_type};
    }

    void put(int idx, expr_t* expr) override {
        assert(!idx);
        assert(expr->type == (is_left ? type->either.left_type : type->either.right_type));
        value = expr;
    }

    expr_t* build() const override {
        return builder.either_expr(value, is_left ? type->either.right_type : type->either.left_type, is_left);
    }

private:
    bool is_left;
    expr_t* value;
};

class SwitchExpr : PartialExpr {
public:
    SwitchExpr(ExprBuilder& builder, type_t* type, type_t* either_type) :
        PartialExpr(builder, type), either_type(either_type) {
        assert(either_type->kind == EITHER_KIND);
        map_left_type = builder.func_type(type, either_type->either.left_type);
        map_right_type = builder.func_type(type, either_type->either.right_type);
        either = nullptr;
        map_left = nullptr;
        map_right = nullptr;
    }

    const operation_t operation() const override {
        return SWITCH;
    }

    std::vector<type_t*> slots() const override {
        return {
            either_type,
            map_left_type,
            map_right_type,
        };
    }

    void put(int idx, expr_t* expr) override {
        switch (idx) {
        case 0:
            assert(expr->type == either_type);
            either = expr;
            break;
        case 1:
            assert(expr->type == map_left_type);
            map_left = expr;
            break;
        case 2:
            assert(expr->type == map_right_type);
            map_right = expr;
            break;
        default:
            assert(false);
        }
    }

    expr_t* build() const override {
        return builder.switch_expr(either, map_left, map_right);
    }

private:
    type_t* either_type;
    type_t* map_left_type;
    type_t* map_right_type;
    expr_t* either;
    expr_t* map_left;
    expr_t* map_right;
};

class PrimitiveExpr : PartialExpr {
public:
    PrimitiveExpr(ExprBuilder& builder, type_t* type) :
        PartialExpr(builder, type), _bool(false), _int(0), ref_type(nullptr) {
        assert(type->kind == BOOL_KIND || type->kind == INT_KIND || type->kind == TYPE_KIND || type->kind == LIST_KIND);
    }

    void set_bool(bool v) {
        assert(type->kind == BOOL_KIND);
        _bool = v;
    }

    void set_int(int v) {
        assert(type->kind == INT_KIND);
        _int = v;
    }

    void set_type(type_t* type) {
        assert(type->kind == TYPE_KIND);
        ref_type = type;
    }

    const operation_t operation() const override {
        return PRIMITIVE;
    }

    std::vector<type_t*> slots() const override {
        return {};
    }

    void put(int idx, expr_t* expr) override {
        assert(false);
    }

    expr_t* build() const override {
        switch (type->kind) {
        case BOOL_KIND:
            return builder.bool_primitive(_bool);
        case INT_KIND:
            return builder.int_primitive(_int);
        case TYPE_KIND:
            return builder.type_expr(ref_type);
        case LIST_KIND:
            return builder.empty(type->list.element_type);
        default:
            assert(false);
        }
    }

private:
    bool _bool;
    int _int;
    type_t* ref_type;
};

class ReferenceExpr : PartialExpr {
public:
    ReferenceExpr(ExprBuilder& builder, type_t * type) :
        PartialExpr(builder, type) {
        index = 0;
    }

    void set_index(int idx) {
        this->index = idx;
    }

    const operation_t operation() const override {
        return REFERENCE;
    }

    std::vector<type_t*> slots() const override {
        return {};
    }

    void put(int idx, expr_t* expr) override {
        assert(false);
    }

    expr_t* build() const override {
        return builder.ref_expr(type, index);
    }

private:
    int index;
};


#endif //ARC_PARTIALEXPR_H
