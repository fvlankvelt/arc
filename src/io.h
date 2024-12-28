#ifndef __IO_H__
#define __IO_H__

#include "task.h"

task_t* parse_task(const char* source);

typedef struct _task_def {
    struct _task_def* next;
    const char* name;
} task_def_t;

task_def_t* list_tasks();
void free_task_list(task_def_t * list);

#endif  // __IO_H__