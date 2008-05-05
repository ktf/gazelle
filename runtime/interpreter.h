/*********************************************************************

  Gazelle: a system for building fast, reusable parsers

  interpreter.h

  This file presents the public API for loading compiled grammars and
  parsing text using Gazelle.  There are a lot of structures, but they
  should all be considered read-only.

  Copyright (c) 2007 Joshua Haberman.  See LICENSE for details.

*********************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include "bc_read_stream.h"

struct parse_state;
typedef void (*parse_callback_t)(struct parse_state *state, void *user_data);

/*
 * RTN
 */

struct rtn_state;
struct rtn_transition;

struct rtn
{
    char *name;
    int num_slots;

    int num_ignore;
    char **ignore_terminals;

    int num_states;
    struct rtn_state *states;  /* start state is first */

    int num_transitions;
    struct rtn_transition *transitions;
};

struct rtn_transition
{
    enum {
      TERMINAL_TRANSITION,
      NONTERM_TRANSITION,
    } transition_type;

    union {
      char            *terminal_name;
      struct rtn      *nonterminal;
    } edge;

    struct rtn_state *dest_state;
    char *slotname;
    int slotnum;
};

struct rtn_state
{
    bool is_final;

    enum {
      STATE_HAS_INTFA,
      STATE_HAS_GLA,
      STATE_HAS_NEITHER
    } lookahead_type;

    union {
      struct intfa *state_intfa;
      struct gla *state_gla;
    } d;

    int num_transitions;
    struct rtn_transition *transitions;
};

/*
 * GLA
 */

struct gla_state;
struct gla_transition;

struct gla
{
    int num_states;
    struct gla_state *states;   /* start state is first */

    int num_transitions;
    struct gla_transition *transitions;
};

struct gla_transition
{
    char *term;
    struct gla_state *dest_state;
};

struct gla_state
{
    bool is_final;

    union {
        struct nonfinal_info {
            struct intfa *intfa;
            int num_transitions;
            struct gla_transition *transitions;
        } nonfinal;

        struct final_info {
            int num_rtn_transitions;
            int *rtn_transition_offsets;  /* 1-based -- 0 is "return" */
        } final;
    } d;
};

/*
 * IntFA
 */

struct intfa_state;
struct intfa_transition;

struct intfa
{
    int num_states;
    struct intfa_state *states;    /* start state is first */

    int num_transitions;
    struct intfa_transition *transitions;
};

struct intfa_transition
{
    int ch_low;
    int ch_high;
    struct intfa_state *dest_state;
};

struct intfa_state
{
    char *final;  /* NULL if not final */
    int num_transitions;
    struct intfa_transition *transitions;
};

struct grammar
{
    char         **strings;

    int num_rtns;
    struct rtn   *rtns;

    int num_glas;
    struct gla   *glas;

    int num_intfas;
    struct intfa *intfas;
};

/*
 * runtime state
 */

struct terminal
{
    int offset;
    int len;
};

struct parse_val;

struct slotarray
{
    struct rtn *rtn;
    int num_slots;
    struct parse_val *slots;
};

struct parse_val
{
    enum {
      PARSE_VAL_EMPTY,
      PARSE_VAL_TERMINAL,
      PARSE_VAL_NONTERM,
      PARSE_VAL_USERDATA
    } type;

    union {
      struct terminal terminal;
      struct slotarray *nonterm;
      char userdata[8];
    } val;
};

struct parse_stack_frame
{
    enum {
      FRAME_TYPE_RTN,
      FRAME_TYPE_GLA,
      FRAME_TYPE_INTFA
    } frame_type;

    union {
      struct {
        struct rtn            *rtn;
        struct rtn_state      *rtn_state;
        struct rtn_transition *rtn_transition;
        struct slotarray      slots;
        int start_offset;
      } rtn_frame;

      struct {
        struct gla            *gla;
        struct gla_state      *gla_state,
        int                   start_offset;
      } gla_frame;

      struct {
        struct intfa          *intfa;
        struct intfa_state    *intfa_state;
        int                   start_offset;
        int                   last_match_offset;
        struct intfa_state    *last_match_state;
      } intfa_frame;
    } f;
};

struct buffer
{
    FILE *file;
    unsigned char *buf;
    int len;
    int size;
    int base_offset;
    bool is_eof;
};

struct completion_callback
{
    char *rtn_name;
    parse_callback_t callback;
};

/* This structure defines the core state of a parsing stream.  By saving this
 * state alone, we can resume a parse from the position where we left off. */
struct parse_state
{
    /* Our current offset in the stream.  We use this to mark the offsets
     * of all the tokens we lex. */
    int offset;

    /* The parse stack is the main piece of state that the parser keeps.
     * There is a stack frame for every RTN, GLA, and IntFA state we are
     * currently in.
     *
     * TODO: The right input can make this grow arbitrarily, so we'll need
     * built-in limits to avoid infinite memory consumption. */
    DEFINE_DYNARRAY(parse_stack, struct parse_stack_frame);

    /* The token buffer is where GLA states store the tokens that they
     * lex.  Once the GLA reaches a final state, it will run this sequence
     * of terminals through RTN transitions, and keeping those terminals
     * here prevents us from having to re-lex them.
     *
     * TODO: If the grammar is LL(k) for fixed k, the token buffer will never
     * need to be longer than k elements long.  If the grammar is LL(*),
     * this can grow arbitrarily depending on the input, and we'll need
     * a way to clamp its maximum length to prevent infinite memory
     * consumption. */
    DEFINE_DYNARRAY(token_buffer, struct terminal);
};

    /* Slotbufs are where each RTN on the parse stack stores information
     * that clients can use to get the results from the parse. */
    DEFINE_DYNARRAY(slot_stack, struct parse_val);

    int num_completion_callbacks;
    struct completion_callback *callbacks;
    void *user_data;
};

struct grammar *load_grammar(struct bc_read_stream *s);
void free_grammar(struct grammar *g);

/* Begin or continue a parse using grammar g, with the current state of the
 * parse represented by s.  It is expected that the text in buf represents the
 * input file or stream at offset s->offset. */
enum parse_status {
  PARSE_STATUS_OK,
  PARSE_STATUS_CANCELLED,
  PARSE_STATUS_EOF
}
enum parse_status parse(struct grammar *g, struct parse_state *s,
                        char *buf, int buf_len, bool eof,
                        int *out_consumed_buf_len);

void alloc_parse_state(struct parse_state *state);
void free_parse_state(struct parse_state *state);
void init_parse_state(struct parse_state *state, struct grammar *g, FILE *file);
void reinit_parse_state(struct parse_state *state);

void register_callback(struct parse_state *state, char *rtn_name, parse_callback_t callback, void *user_data);

/*
 * Local Variables:
 * c-file-style: "bsd"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:et:sts=4:sw=4
 */
