/*
 * Copyright (c) 2001 Michael B. Allen <mballen@erols.com>
 *
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */ 

/* stack.h - a dynamically resizing stack
 */

#ifndef STACK_H
#define STACK_H

namespace ATIXML
{

struct stack;
struct stack *stack_new(unsigned int max_size);
void stack_del(struct stack *s, void (*free_data_fn)(void *));

void stack_clear(struct stack *s, void (*free_data_fn)(void *));
int stack_push(struct stack *s, void *data);
void *stack_pop(struct stack *s);
int stack_is_empty(const struct stack *s);
unsigned int stack_size(const struct stack *s);
void stack_iterate(struct stack *s);
void *stack_next(struct stack *s);
void *stack_peek(struct stack *s);

}

#endif /* STACK_H */
