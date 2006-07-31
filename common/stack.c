/* stack.c - Implementation of generic stack

   Copyright (C) 2006 Jonathan Dion <dion.jonathan@gmail.com>
   
   This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

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
