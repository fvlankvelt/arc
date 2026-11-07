#include <stdio.h>

#include "../test.h"
#include "../../src/pola/ast.h"
#include "pola/ExprBuilder.h"

BEGIN_TEST(test_simple)
    {
        ExprBuilder builder;
        expr_t* result =
            builder.eval_expr(
                builder.func_expr(
                    builder.bool_type(),
                    builder.if_expr(
                        builder.ref_expr(builder.bool_type(), 0),
                        builder.int_primitive(1),
                        builder.int_primitive(0)
                    )
                ),
                builder.bool_primitive(true)
            );
        ASSERT(validate_expression(result), "expression did not validate");
        mem_block_t* values = new_block(256, sizeof(value_t));
        value_t* value = evaluate_expression(result, values);
        ASSERT(value->primitive.integer == 1, "expected 1");
        ASSERT(value->ref_count == 1, "expected ref_count 1");
        free_block(values);
    }
END_TEST()

BEGIN_TEST(test_list)
    {
        ExprBuilder builder;
        expr_t* empty_list = builder.empty(builder.bool_type());
        expr_t* result =
            builder.elem_expr(
                builder.cons_expr(builder.bool_primitive(true), empty_list),
                builder.int_primitive(0),
                builder.func_expr(
                    builder.bool_type(),
                    builder.func_expr(empty_list->type, builder.int_primitive(1))
                )
            );
        ASSERT(validate_expression(result), "expression did not validate");
        mem_block_t* values = new_block(256, sizeof(value_t));
        value_t* value = evaluate_expression(result, values);
        ASSERT(value->primitive.integer == 1, "expected 1");
        ASSERT(value->ref_count == 1, "expected ref_count 1");
        free_block(values);
    }
END_TEST()

BEGIN_TEST(test_complex)
    {
        ExprBuilder builder;
        type_t* identity_factory_type = builder.func_type(
            builder.func_type(
                builder.ref_type(0),
                builder.ref_type(0)
            ),
            builder.type_type()
        );
        expr_t* result =
            builder.eval_expr(
                builder.func_expr(
                    identity_factory_type,
                    builder.switch_expr(
                        builder.either_expr(
                            builder.int_primitive(3),
                            builder.bool_type(),
                            false
                        ),
                        builder.func_expr(
                            builder.bool_type(),
                            builder.int_primitive(1)
                        ),
                        builder.eval_expr(
                            builder.ref_expr(identity_factory_type, 0),
                            builder.type_expr(builder.int_type())
                        )
                    )
                ),
                builder.func_expr(
                    builder.type_type(),
                    builder.func_expr(
                        builder.ref_type(0),
                        builder.ref_expr(builder.ref_type(0), 0)
                    )
                )
            );
        builder.expand_types(result, nullptr);
        ASSERT(validate_expression(result), "expression did not validate");
        mem_block_t* values = new_block(256, sizeof(value_t));
        value_t* value = evaluate_expression(result, values);
        ASSERT(value->primitive.integer == 3, "expected 3");
        ASSERT(value->ref_count == 1, "expected ref_count 1");
        free_block(values);
    }
END_TEST()

extern "C" {
DEFINE_SUITE(test_interpreter, {
             RUN_TEST(test_simple);
             RUN_TEST(test_list);
             RUN_TEST(test_complex);
             })
}
