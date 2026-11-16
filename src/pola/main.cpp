#include <ios>
#include <memory>
#include <random>
#include <set>

#include <stdio.h>
#include <unordered_set>

#include "ast.h"
#include "ExprBuilder.h"
#include "PartialExpr.h"

#include "../util/io.h"

struct Context {
    PartialExpr& expr;
    std::shared_ptr<Context> parent;
};

struct Slot {
    type_t* type;
    int index;
    std::shared_ptr<Context> context;
};

struct Candidate {
    std::shared_ptr<Slot> slot;
    std::shared_ptr<PartialExpr> expr;
    auto operator<=>(const Candidate&) const = default;
};

std::set<Candidate> create_candidates(const std::shared_ptr<Slot> slot, ExprBuilder& builder) {
    std::set<Candidate> candidates;
    switch (slot->type->kind) {
    case BOOL_KIND:
        {
            std::shared_ptr<PrimitiveExpr> true_expr(new PrimitiveExpr(builder, slot->type));
            true_expr->set_bool(true);
            candidates.insert({.slot = slot, .expr = true_expr});

            std::shared_ptr<PrimitiveExpr> false_expr(new PrimitiveExpr(builder, slot->type));
            false_expr->set_bool(false);
            candidates.insert({.slot = slot, .expr = false_expr});
        }
        break;
    case INT_KIND:
        for (int i = 0; i <= 10; i++) {
            std::shared_ptr<PrimitiveExpr> int_expr(new PrimitiveExpr(builder, slot->type));
            int_expr->set_int(i);
            candidates.insert({.slot = slot, .expr = int_expr});
        }
        break;
    case TYPE_KIND:
        {
            for (auto& type : builder.list_types()) {
                std::shared_ptr<PrimitiveExpr> type_expr(new PrimitiveExpr(builder, slot->type));
                type_expr->set_type(type);
                candidates.insert({.slot = slot, .expr = type_expr});
            }
        }
        break;
    case TYPE_REFERENCE_KIND:
        break;
    case FUNC_KIND:
        {
            std::shared_ptr<FuncExpr> expr(new FuncExpr(builder, slot->type));
            candidates.insert({.slot = slot, .expr = expr});
        }
        break;
    case LIST_KIND:
        {
            std::shared_ptr<PrimitiveExpr> nil_expr(new PrimitiveExpr(builder, slot->type));
            candidates.insert({.slot = slot, .expr = nil_expr});
            std::shared_ptr<ConsExpr> cons_expr(new ConsExpr(builder, slot->type->list.element_type));
            candidates.insert({.slot = slot, .expr = cons_expr});
        }
        break;
    case PAIR_KIND:
        {
            std::shared_ptr<PairExpr> nil_expr(new PairExpr(builder, slot->type));
            candidates.insert({.slot = slot, .expr = nil_expr});
        }
        break;
    case EITHER_KIND:
        {
            std::shared_ptr<EitherExpr> left_expr(new EitherExpr(builder, slot->type, true));
            candidates.insert({.slot = slot, .expr = left_expr});
            std::shared_ptr<EitherExpr> right_expr(new EitherExpr(builder, slot->type, false));
            candidates.insert({.slot = slot, .expr = right_expr});
        }
        break;
    }

    // add references to arguments on the stack
    std::shared_ptr context(slot->context);
    int ref_counter = 0;
    while (context) {
        if (context->expr.operation() == FUNC) {
            const type_t* type = context->expr.get_type();
            if (type->function.argument_type == slot->type) {
                std::shared_ptr<ReferenceExpr> ref_expr(new ReferenceExpr(builder, slot->type));
                ref_expr->set_index(ref_counter);
                candidates.insert({.slot = slot, .expr = ref_expr});
                ref_counter++;
            }
        }
        context = context->parent;
    }

    // as left/right from a pair
    for (auto& type : builder.list_types()) {
        if (type->kind != PAIR_KIND) {
            continue;
        }
        if (type->pair.left_type == slot->type) {
            std::shared_ptr<LeftRightExpr> left_expr(new LeftRightExpr(builder, LEFT, type));
            candidates.insert({.slot = slot, .expr = left_expr});
        }
        if (type->pair.right_type == slot->type) {
            std::shared_ptr<LeftRightExpr> right_expr(new LeftRightExpr(builder, RIGHT, type));
            candidates.insert({.slot = slot, .expr = right_expr});
        }
    }

    // from an either
    for (auto& type : builder.list_types()) {
        if (type->kind != EITHER_KIND) {
            continue;
        }
        std::shared_ptr<SwitchExpr> switch_expr(new SwitchExpr(builder, slot->type, type));
        candidates.insert({.slot = slot, .expr = switch_expr});
    }

    return candidates;
}

// embedding a candidate
// at each level (from the tree)
// - type, operation, index
// embed types with a recursive neural network
// i.e. learn a map from 2n -> n features for Either and Pair types.

int main(int argc, char* argv[]) {
    FILE* out = stdout;
    if (argc >= 2) {
        char* out_filename = argv[1];
        out = fopen(out_filename, "a");
    }

    task_def_t* tasks = list_tasks();
    int n_tasks = 0;
    for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
        const char* source = read_task(task_def->name);
        if (source) {
            task_def->task = parse_task(source);
            n_tasks++;
        }
    }
    task_def_t* task_array[n_tasks];
    n_tasks = 0;
    for (task_def_t* task_def = tasks; task_def; task_def = task_def->next) {
        if (task_def->task) {
            task_array[n_tasks++] = task_def;
        }
    }

    std::mt19937 rng;

    // fprintf(out, "task,example,loss,reconstructed\n");
    while (true) {
        std::uniform_int_distribution<std::size_t> task_dist(0, n_tasks - 1);
        auto i_task = task_dist(rng);
        task_def_t* task_def = task_array[i_task];

        ExprBuilder builder;
        type_t* grid_type = builder.either_type(
            builder.either_type(builder.int_type(), builder.int_type()),
            builder.list_type(builder.list_type(builder.int_type()))
        );
        type_t* transformation_type = builder.func_type(
            grid_type,
            grid_type
        );
        std::vector<std::unique_ptr<Slot>> slots;
        FuncExpr start(builder, transformation_type);
        Context context(start, nullptr);
        auto start_slots = start.slots();
        for (auto index = 0; index < start_slots.size(); index++) {
            slots.emplace_back(new Slot(start_slots[index], index, start));
        }
        while (!slots.empty()) {
            // TODO: replace with a policy generated by the transformer
            // A particularly elegant solution might be to directly compute a softmax over all
            // candidates for all slots.
            // input to the transformer is all (input, output) training pairs and test inputs.
            std::uniform_int_distribution<std::size_t> slot_dist(0, slots.size() - 1);
            auto it = slots.begin() + slot_dist(rng);
            std::unique_ptr<Slot> slot = std::move(*it);
            slots.erase(it);
            // fill in slot
        }
        expr_t* function = start.build();
    }
}
