/*! 
 * @file stack.h
 * @brief Implementation of generic stack
 * 
 * @author Copyright (C) 2006 Jonathan Dion <dion.jonathan@gmail.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef STACK_H_	// To prevent circular inclusion
#define STACK_H_

/** 
 * Basic structure of a stack
 * 
 * @param data : array of pointer piled up
 * @param front : indice of the front pointer. -1 if empty stack
 * @param stack_size : the max number of pointer that can be piled up.
 */
typedef struct _t_stack {
  void** data;
  int front;
  int stack_size;
} *t_stack;

/** 
 * Create and initialize a new stack
 * 
 * @param stack_size the max size of the stack to create
 * @return the new stack
 */
t_stack new_stack(int stack_size);

/**
 * Push a pointer on a stack
 * 
 * @param data The pointer to pile up
 * @param stack The stack you want to push the pointer to
 * 
 * @return 	1 if there were a problem (stack was full), 0 else
 */
int stack_push(void* data, t_stack stack);


/**
 * Pop a pointer from a stack
 * 
 * @param stack The stack you want to pop
 * @return the poped pointer
 */
void* stack_pop(t_stack stack);

/**
 * Access to the front pointer of a stack
 * 
 * @param stack The stack you want to get the front pointer of
 * @return the front pointer
 */
void* stack_front(t_stack stack);

/**
 * Is a stack empty ?
 * 
 * @param stack The stack you want to know if it is empty
 * @return 1 if the stack is empty, else 0
 */
int stack_is_empty(t_stack stack);

/**
 * Is a stack full ?
 * 
 * @param stack The stack you want to know if it is full
 * @return 1 if the stack is full, else 0
 */
int stack_is_full(t_stack stack);

void free_stack(t_stack stack);

#endif /*STACK_H_*/
