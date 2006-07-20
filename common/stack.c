#include <stdlib.h>

#include "stack.h"
#include "common.h"



t_stack new_stack(int stack_size) {
  t_stack stack;
  stack = xmalloc(sizeof(struct _t_stack));
  stack->data = xmalloc(sizeof(void*) * stack_size );
  stack->front = -1;
  stack->stack_size = stack_size;
  return stack;
}

int stack_push(void* data, t_stack stack) {
	if (stack->front == stack->stack_size) {
		return 1;
	}
	stack->front++;
	stack->data[stack->front] = data;
	return 0;
}

void* stack_pop(t_stack stack) {
	if (stack->front == -1) {
		return (void*)0;
	}
	return stack->data[stack->front--];
}

void* stack_front(t_stack stack) {
	if (stack->front == -1) {
		return (void*)0;
	}
	return stack->data[stack->front];
}

int stack_is_empty(t_stack stack) {
	if (stack->front == -1) {
		return 1;
	}
	return 0;
}

int stack_is_full(t_stack stack) {
	if (stack->front == (stack->stack_size - 1)) {
		return 1;
	}
	return 0;
}



void free_stack(t_stack stack) {
	free(stack->data);
	free(stack);
}
