#include <ios>
#include <memory>

#include <stdio.h>

#include "ast.h"
#include "ExprBuilder.h"
#include "PartialExpr.h"

#include "../util/io.h"
#include "../util/mtwister.h"

struct ArgumentStack {
    type_t* type;
    ArgumentStack* next;
};

struct Slot {
    type_t * type;
    int index;
    PartialExpr& expr;
};

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

    MTRand rnd = seedRand(1234l);

    fprintf(out, "task,example,loss,reconstructed,abstraction,filter,transform\n");
    while (true) {
        int i_task = genRandLong(&rnd) % n_tasks;
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
        auto start_slots = start.slots();
        for (auto index =0; index < start_slots.size(); index++) {
            slots.emplace_back(new Slot(start_slots[index], index, start));
        }
        while (!slots.empty()) {
            // TODO: fill first slot
            slots.front()
            unique_ptr<Slot> slot = slots.pop_back();
        }
    }
}
