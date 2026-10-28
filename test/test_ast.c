#include <stdbool.h>
#include <stdio.h>

#include "test.h"
#include "ast.h"

BEGIN_TEST(test_primitive)
    {
        type_t type_bool = {
            .kind = BOOL_KIND,
        };
        expr_t expr = {
            .type = &type_bool,
            .operation = PRIMITIVE,
            .value = {
                .primitive = {
                    .boolean = true,
                }
            }
        };
        ASSERT(validate_expression(&expr), "Boolean primitive expression failed to validate");
    }
END_TEST()

BEGIN_TEST(test_list)
    {
        type_t type_bool = {
            .kind = BOOL_KIND,
        };
        type_t type_int = {
            .kind = INT_KIND,
        };
        type_t type_list_bool = {
            .kind = LIST_KIND,
            .arguments = {
                .list = {
                    .element_type = &type_bool,
                }
            }
        };
        expr_t nil = {
            .type = &type_list_bool,
            .operation = PRIMITIVE,
        };
        expr_t entry = {
            .type = &type_bool,
            .operation = PRIMITIVE,
            .value = {
                .primitive = {
                    .boolean = true,
                }
            }
        };
        expr_t list = {
            .type = &type_list_bool,
            .operation = CONS,
            .value = {
                .cons = {
                    .head = &entry,
                    .tail = &nil,
                }
            }
        };
        expr_t empty = {
            .type = &type_int,
            .operation = PRIMITIVE,
            .value = {
                .primitive = {
                    .integer = 0,
                }
            }
        };

        type_t nonempty_curried_type = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .argument_type = &type_list_bool,
                    .return_type = &type_int
                }
            }
        };
        expr_t nonempty_curried_body = {
            .type = &type_int,
            .operation = PRIMITIVE,
            .value = {
                .primitive = {
                    .integer = 1,
                }
            }
        };
        expr_t nonempty_curried = {
            .type = &nonempty_curried_type,
            .operation = FUNC,
            .value = {
                .func = {
                    .body = &nonempty_curried_body,
                }
            }
        };
        type_t nonempty_type = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .return_type = &nonempty_curried_type,
                    .argument_type = &type_bool,
                }
            }
        };
        expr_t nonempty = {
            .type = &nonempty_type,
            .operation = FUNC,
            .value = {
                .func = {
                    .body = &nonempty_curried,
                }
            }
        };
        expr_t elem = {
            .type = &type_int,
            .operation = ELEM,
            .value = {
                .elem = {
                    .list = &list,
                    .empty = &empty,
                    .nonempty = &nonempty,
                }
            }
        };
        ASSERT(validate_expression(&elem), "list did not validate");
    }
END_TEST()

/**
 * build (eval (lambda I (switch (either[bool, int] true true) (lambda b (if b 2 2)) (eval I int))) (lambda i:I (i)))
 *
 * I.e. Switch on the argument of an Either - applying the identity function in the second (integer) case
 * The I is the identity factory.  It maps a type argument to the identity function for that type.
 */
BEGIN_TEST(test_switch)
    {
        type_t type_bool = {
            .kind = BOOL_KIND,
        };
        type_t type_int = {
            .kind = INT_KIND,
        };
        type_t type_either = {
            .kind = EITHER_KIND,
            .arguments = {
                .either = {
                    .left_type = &type_bool,
                    .right_type = &type_int,
                }
            }
        };
        type_t type_bool_to_int = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .return_type = &type_int,
                    .argument_type = &type_bool,
                }
            }
        };
        type_t type_int_to_int = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .return_type = &type_int,
                    .argument_type = &type_int,
                }
            }
        };
        expr_t expr_bool = {
            .type = &type_bool,
            .operation = PRIMITIVE,
            .name = "expr_bool",
            .value = {
                .primitive = {
                    .boolean = true,
                }
            }
        };
        expr_t expr_either = {
            .type = &type_either,
            .operation = EITHER,
            .name = "expr_either",
            .value = {
                .either = {
                    .is_left = true,
                    .value = &expr_bool,
                }
            }
        };
        expr_t ref_argument = {
            .type = &type_bool,
            .operation = REFERENCE,
            .name = "ref_argument",
            .value = {
                .reference = {
                    .index = 0
                }
            }
        };
        expr_t constant_int = {
            .type = &type_int,
            .operation = PRIMITIVE,
            .name = "constant_int",
            .value = {
                .primitive = {
                    .integer = 2,
                }
            }
        };
        expr_t map_bool_body = {
            .type = &type_int,
            .operation = IF,
            .name = "map_bool_body",
            .value = {
                ._if = {
                    .predicate = &ref_argument,
                    ._then = &constant_int,
                    ._else = &constant_int,
                }
            }
        };
        expr_t map_bool = {
            .type = &type_bool_to_int,
            .operation = FUNC,
            .name = "map_bool",
            .value = {
                .func = {
                    .body = &map_bool_body,
                }
            }
        };

        // identity: [Type] -> [T: Type] -> [T: Type]

        type_t type_identity_reference = {
            .kind = TYPE_REF_KIND,
            .arguments = {
                .reference = {
                    .index = 0
                }
            }
        };
        type_t type_identity_result = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .return_type = &type_identity_reference,
                    .argument_type = &type_identity_reference,
                }
            }
        };

        // (identity <int (identity fn arg)> <int (result of wrapped fn)>)

        type_t type_identity_argument = {
            .kind = TYPE_ARG_KIND,
        };
        type_t type_identity_factory = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .return_type = &type_identity_result,
                    .argument_type = &type_identity_argument,
                }
            }
        };

        expr_t integer_factory_ref = {
            .type = &type_identity_factory,
            .operation = REFERENCE,
            .name = "integer_factory_ref",
            .value = {
                .reference = {
                    .index = 0
                }
            }
        };
        expr_t integer_factory_argument = {
            .type = &type_identity_argument,
            .operation = PRIMITIVE,
            .name = "integer_factory_argument",
            .value = {
                .primitive = {
                    .type = &type_int,
                }
            }
        };
        expr_t map_integer = {
            .type = &type_int_to_int,
            .operation = EVAL,
            .name = "map_integer",
            .value = {
                .eval = {
                    .function = &integer_factory_ref,
                    .argument = &integer_factory_argument,
                }
            }
        };

        expr_t switch_expr = {
            .type = &type_int,
            .operation = SWITCH,
            .name = "switch",
            .value = {
                ._switch = {
                    .either = &expr_either,
                    .map_left = &map_bool,
                    .map_right = &map_integer,
                }
            }
        };

        type_t type_integer_factory_ref_to_int = {
            .kind = FUNC_KIND,
            .arguments = {
                .function = {
                    .return_type = &type_int,
                    .argument_type = &type_identity_factory,
                }
            }
        };

        expr_t main = {
            .type = &type_integer_factory_ref_to_int,
            .operation = FUNC,
            .name = "main",
            .value = {
                .func = {
                    .body = &switch_expr
                }
            }
        };

        expr_t identity_result_body = {
            .type = &type_identity_reference,
            .operation = REFERENCE,
            .name = "identity_result_body",
            .value = {
                .reference = {
                    .index = 0
                }
            }
        };
        expr_t identity_result = {
            .type = &type_identity_result,
            .operation = FUNC,
            .name = "identity_result",
            .value = {
                .func = {
                    .body = &identity_result_body,
                }
            }
        };
        expr_t identity_factory = {
            .type = &type_identity_factory,
            .operation = FUNC,
            .name = "identity_factory",
            .value = {
                .func = {
                    .body = &identity_result,
                }
            }
        };

        expr_t expr = {
            .type = &type_int,
            .operation = EVAL,
            .name = "final expression",
            .value = {
                .eval = {
                    .function = &main,
                    .argument = &identity_factory,
                }
            }
        };

        ASSERT(validate_expression(&expr), "Complex expression failed to validate");
    }
END_TEST()

DEFINE_SUITE(test_ast, {
             RUN_TEST(test_primitive);
             RUN_TEST(test_list);
             RUN_TEST(test_switch);
             })
