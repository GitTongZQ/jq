#ifndef FRAME_LAYOUT_H
#include "forkable_stack.h"
#include "bytecode.h"
#include <jansson.h>

struct closure {
  struct bytecode* bc;
  stack_idx env;
};

struct continuation {
  struct bytecode* bc;
  stack_idx env;
  uint16_t* retaddr;
};

typedef union frame_elem {
  FORKABLE_STACK_HEADER;
  struct continuation cont;
  struct closure closure;
  json_t* jsonval;
} *frame_ptr;

/*
 * Frame layout
 * fr[0] - FORKABLE_STACK_HEADER (next pointer)
 * fr[1] - self (used to store return addresses, etc)
 * fr[2...nclosures+2] - closure params
 * fr[nclosures+2..nclosures+nlocals+2] - local variables
 */

static int frame_size(struct bytecode* bc) {
  return sizeof(union frame_elem) * (bc->nclosures + bc->nlocals + 2);
}

static struct continuation* frame_self(frame_ptr fr) {
  return &fr[1].cont;
}

static struct closure* frame_closure_arg(frame_ptr fr, int closure) {
  assert(closure >= 0);
  assert(closure < frame_self(fr)->bc->nclosures);
  return &fr[2+closure].closure;
}

static json_t** frame_local_var(frame_ptr fr, int var) {
  assert(var >= 0);
  assert(var < frame_self(fr)->bc->nlocals);
  return &fr[2 + frame_self(fr)->bc->nclosures + var].jsonval;
}


static frame_ptr frame_current(struct forkable_stack* stk) {
  frame_ptr fp = forkable_stack_peek(stk);
  if (forkable_stack_peek_next(stk, fp)) {
    struct bytecode* bc = frame_self(forkable_stack_peek_next(stk, fp))->bc;
    assert(frame_self(fp)->retaddr >= bc->code && frame_self(fp)->retaddr < bc->code + bc->codelen);
  } else {
    assert(frame_self(fp)->retaddr == 0);
  }
  return fp;
}

static struct bytecode* frame_current_bytecode(struct forkable_stack* stk) {
  return frame_self(frame_current(stk))->bc;
}
static uint16_t** frame_current_retaddr(struct forkable_stack* stk) {
  return &frame_self(frame_current(stk))->retaddr;
}

static frame_ptr frame_get_parent(struct forkable_stack* stk, frame_ptr fr) {
  return forkable_stack_from_idx(stk, frame_self(fr)->env);
}

static frame_ptr frame_get_level(struct forkable_stack* stk, frame_ptr fr, int level) {
  for (int i=0; i<level; i++) {
    fr = frame_get_parent(stk, fr);
  }
  return fr;
}

static struct closure closure_new_toplevel(struct bytecode* bc) {
  struct closure cl = {bc, -1};
  return cl;
}

static struct closure closure_new(struct forkable_stack* stk, struct bytecode* bc) {
  struct closure cl = {bc, 
                       forkable_stack_to_idx(stk, frame_current(stk))};
  return cl;
}

static frame_ptr frame_push(struct forkable_stack* stk, struct closure cl, uint16_t* retaddr) {
  frame_ptr fp = forkable_stack_push(stk, frame_size(cl.bc));
  struct continuation* cc = frame_self(fp);
  cc->bc = cl.bc;
  cc->env = cl.env;
  cc->retaddr = retaddr;
  return fp;
}

static frame_ptr frame_push_backtrack(struct forkable_stack* stk, uint16_t* retaddr) {
  struct continuation cc = *frame_self(frame_current(stk));
  frame_ptr fp = forkable_stack_push(stk, sizeof(union frame_elem) * 2);
  cc.retaddr = retaddr;
  *frame_self(fp) = cc;
  return fp;
}

static void frame_pop(struct forkable_stack* stk) {
  forkable_stack_pop(stk);
}

#endif