#pragma once

// Singly-linked stack helpers.
#define SLL_STACK_PUSH_N(f, n, next) ((n)->next = (f), (f) = (n))
#define SLL_STACK_POP_N(f, next) ((f) = (f)->next)
#define SLL_STACK_PUSH(f, n) SLL_STACK_PUSH_N(f, n, next)
#define SLL_STACK_POP(f) SLL_STACK_POP_N(f, next)
