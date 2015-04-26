/* Function prototypes

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2015 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or (at your
	option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
	for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <http://www.gnu.org/licenses/>.  */


/* actions.c */
int do_action(buffer *b, action a, int64_t c, char *p);

/* autocomp.c */
char *autocomplete(char *p, char *req_msg, const int ext, int * const error);

/* buffer.c */
encoding_type detect_buffer_encoding(const buffer *b);
char_pool *alloc_char_pool(int64_t size);
void free_char_pool(char_pool *cp);
char_pool *get_char_pool(buffer *b, char * const p);
line_desc_pool *alloc_line_desc_pool(int64_t pool_size);
void free_line_desc_pool(line_desc_pool *ldp);
buffer *alloc_buffer(const buffer *cur_b);
void free_buffer_contents(buffer *b);
void clear_buffer(buffer *b);
void free_buffer(buffer *b);
int64_t calc_lost_chars(const buffer *b);
buffer *get_nth_buffer(int n);
buffer *get_buffer_named(const char *p);
int modified_buffers(void);
int save_all_modified_buffers(void);
line_desc *alloc_line_desc(buffer *b);
void free_line_desc(buffer *b, line_desc *ld);
char *alloc_chars(buffer *b, int64_t len);
int64_t alloc_chars_around(buffer *b, line_desc *ld, int64_t n, bool check_first_before);
void free_chars(buffer *b, char *p, int64_t len);
int insert_one_line(buffer *b, line_desc *ld, int64_t line, int64_t pos);
int delete_one_line(buffer *b, line_desc *ld, int64_t line);
int undelete_line(buffer *b);
void delete_to_eol(buffer *b, line_desc *ld, int64_t line, int64_t pos);
int insert_stream(buffer *b, line_desc *ld, int64_t line, int64_t pos, const char *stream, int64_t stream_len);
int insert_one_char(buffer *b, line_desc *ld, int64_t line, int64_t pos, int c);
int insert_spaces(buffer *b, line_desc *ld, int64_t line, int64_t pos, int64_t n);
int delete_stream(buffer *b, line_desc *ld, int64_t line, int64_t pos, int64_t len);
int delete_one_char(buffer *b, line_desc *ld, int64_t line, int64_t pos);
void change_filename(buffer *b, char *name);
void ensure_attr_buf(buffer * const b, const int64_t capacity);
int load_file_in_buffer(buffer *b, const char *name);
int load_fh_in_buffer(buffer *b, int fh);
int save_buffer_to_file(buffer *b, const char *name);
void auto_save(buffer *b);
void reset_syntax_states(buffer *b);

/* clips.c */
clip_desc *alloc_clip_desc(int n, int64_t size);
clip_desc *realloc_clip_desc(clip_desc *cd, int n, int64_t size);
void free_clip_desc(clip_desc *cd);
int is_encoding_neutral(clip_desc *cd);
clip_desc *get_nth_clip(int n);
int copy_to_clip(buffer *b, int n, bool cut);
int erase_block(buffer *b);
int paste_to_buffer(buffer *b, int n);
int copy_vert_to_clip(buffer *b, int n, bool cut);
int erase_vert_block(buffer *b);
int paste_vert_to_buffer(buffer *b, int n);
int load_clip(int n, const char *name, bool preserve_cr, bool binary);
int save_clip(int n, const char *name, bool CRLF, bool binary);

/* command.c */
void build_hash_table(void);
void build_command_name_table(void);
int parse_command_line(const char *command_line, int64_t *num_arg, char **string_arg, int exec_only_options);
int execute_command_line(buffer *b, const char *command_line);
macro_desc *alloc_macro_desc(void);
void free_macro_desc(macro_desc *md);
void record_action(char_stream *cs, action a, int64_t c, const char *p, bool verbose);
int play_macro(buffer *b, char_stream *cs);
macro_desc *load_macro(const char *name);
int execute_macro(buffer *b, const char *name);
void help(char *p);
int cmdcmp(const char *c, const char *m);
void unload_macros(void);

void optimize_macro(char_stream *cs, int verbose);

/* display.c */
void update_syntax_states(buffer *b, int row, line_desc *ld, line_desc *end_ld);
int highlight_cmp(HIGHLIGHT_STATE *x, HIGHLIGHT_STATE *y);
void delay_update();
void output_line_desc(int row, int col, line_desc *ld, int64_t start, int64_t len, int tab_size, bool cleared_at_end, bool utf8, const uint32_t * const attr, const uint32_t * const diff, const int64_t diff_size);
line_desc *update_partial_line(buffer *b, int n, int64_t start_x, bool cleared_at_end, const bool differential);
void update_line(buffer *b, int n, const bool cleared_at_end, const bool differential);
void update_window_lines(buffer *b, int start_line, int end_line, bool doit);
void update_syntax_and_lines(buffer *b, line_desc *start_ld, line_desc *end_ld);
void update_window(buffer *b);
void update_deleted_char(buffer *b, int c, int a, const line_desc *ld, int64_t pos, int64_t attr_pos, int line, int x);
void update_inserted_char(buffer *b, int c, const line_desc *ld, int64_t pos, int64_t attr_pos, int line, int x);
void update_overwritten_char(buffer *b, int old_char, int new_char, const line_desc *ld, int64_t pos, int64_t attr_pos, int line, int x);
void reset_window(void);
void refresh_window(buffer *b);
void scroll_window(buffer *b, int line, int n);
HIGHLIGHT_STATE freeze_attributes(buffer *b, line_desc *ld);
void automatch_bracket(buffer * const b, const bool show);

/* edit.c */
int to_upper(buffer *b);
int to_lower(buffer *b);
int capitalize(buffer *b);
int match_bracket(buffer *b);
int find_matching_bracket(buffer *b, const int64_t min_line, const int64_t max_line, int64_t *match_line, int64_t *match_pos, int *c, line_desc ** ld);
int word_wrap(buffer *b);
int paragraph(buffer *b);
int center(buffer *b);
int auto_indent_line(buffer * const b, const int64_t line, line_desc * const ld, const int64_t up_to_col);
int backtab(buffer *b);
int shift(buffer *b, char *p, char *msg, int msg_size);

/* errors.c */

/* exec.c */
void new_list(list *l);
void add_head(list *l, node *n);
void add_tail(list *l, node *n);
void rem(node *n);
void add(node *n, node *pos);
void free_list(list *l, void (func)());
void apply_to_list(list *l, void (func)());

/* ext.c */
const char *ext2syntax(const char * const ext);

/* help.c */

/* inputclass.c */

/* keys.c */
void read_key_capabilities(void);
void set_escape_time(int new_escape_time);
int get_key_code(void);
int key_may_set(const char * const cap_string, int code);

/* menu.c */
void print_message(const char *message);
int search_menu_title(int n, int c);
int search_menu_item(int n, int c);
void reset_status_bar(void);
char *gen_flag_string(const buffer *b);
void draw_status_bar(void);
int print_error(int error_num);
void print_info(int info_num);
void alert(void);
void handle_menus(void);
void get_menu_configuration(const char *);
void get_key_bindings(const char *);

/* names.c */

/* navigation.c */
int  adjust_view(buffer *b, const char *p);
int  char_left(buffer *b);
int  char_right(buffer *b);
int  line_down(buffer *b);
int  line_up(buffer *b);
int  move_bos(buffer *b);
int  move_tos(buffer *b);
int  next_page(buffer *b);
int  page_down(buffer *b);
int  page_up(buffer *b);
int  prev_page(buffer *b);
void goto_column(buffer *b, int64_t n);
void goto_line(buffer *b, int64_t n);
void goto_pos(buffer *b, int64_t pos);
void keep_cursor_on_screen(buffer *b);
void move_to_bof(buffer *b);
void move_to_eol(buffer *b);
void move_to_sof(buffer *b);
void move_to_sol(buffer *b);
void reset_position_to_sof(buffer *b);
void resync_pos(buffer * const b);
void toggle_sof_eof(buffer *b);
void toggle_sol_eol(buffer *b);
int  search_word(buffer *b, int dir);
void move_to_eow(buffer *b);
void move_inc_down(buffer *b);
void move_inc_up(buffer *b);

/* ne.c */
buffer *new_buffer(void);
int delete_buffer(void);
void about(bool show);
void automatch_bracket(buffer *b, bool show);
int main(int argc, char **argv);

/* prefs.c */
const char *extension(const char *filename);
char *exists_prefs_dir(void);
char *exists_gprefs_dir(void);
int   save_prefs(buffer *b, const char *name);
int   load_prefs(buffer *b, const char *name);
int   load_syntax_by_name(buffer *b, const char *name);
int   load_auto_prefs(buffer *b, const char *name);
int   save_auto_prefs(buffer *b, const char *name);
int   pop_prefs(buffer *b);
int   push_prefs(buffer *b);

/* input.c */
void  close_history(void);
bool  request_response(const buffer *b, const char *prompt, int default_value);
char  request_char(const buffer *b, const char *prompt, const char default_value);
int64_t request_number(const char *prompt, int64_t default_value);
char *request_string(const char *prompt, const char *default_string, bool accept_null_string, int completion_allowed, bool prefer_utf8);
char *request(const char *prompt, const char *default_string, bool alpha_allowed, int completion_allowed, bool prefer_utf8);

/* request.c */
int   request_strings(req_list * const rl, int default_entry);
char *request_syntax();
char *request_files(const char *filename, int use_prefix);
char *request_file(const buffer *b, const char *prompt, const char *default_name);
int   request_document(void);
char *complete_filename(const char *start_prefix);
/* int   req_list_del(req_list * const rl, int nth); */
void  req_list_free(req_list * const rl);
int   req_list_init(req_list * const rl, int cmpfnc(const char *, const char *), const int allow_dupes, const int allow_reorder, const char suffix);
char *req_list_add(req_list * const rl, char * const str, const int suffix);
void  req_list_finalize(req_list * const rl);


/* search.c */
int  find(buffer *b, const char *pattern, const bool skip_first);
int  replace(buffer *b, int n, const char *string);
int  find_regexp(buffer *b, const char *regex, const bool skip_first);
int  replace_regexp(buffer *b, const char *string);

/* signals.c */
void stop_ne(void);
void set_fatal_code(void);
void block_signals(void);
void release_signals(void);
void set_stop(int sig);
void handle_int(int sig);
void handle_winch(int sig);

/* streams.c */
char_stream *alloc_char_stream(int64_t size);
void free_char_stream(char_stream *cs);
char_stream *realloc_char_stream(char_stream *cs, int64_t size);
int add_to_stream(char_stream *cs, const char *s, int64_t len);
char_stream *reset_stream(char_stream *cs);
void set_stream_encoding(char_stream *cs, encoding_type source);
char_stream *load_stream(char_stream *cs, const char *name, bool preserve_cr, bool binary);
char_stream *load_stream_from_fh(char_stream *cs, int fh, bool preserve_cr, bool binary);
int save_stream(const char_stream *cs, const char *name, bool CRLF, bool binary);
int save_stream_to_fh(const char_stream *cs, int fh, bool CRLF, bool binary);

int delete_from_stream(char_stream *cs, int64_t p, int64_t len);
int insert_in_stream(char_stream *cs, const char *s, int64_t p, int64_t len);

/* support.c */
bool same_str(const char *p, const char *q);
char *ne_getcwd(const int bufsize);
const char *get_global_dir(void);
const char *tilde_expand(const char *filename);
const char *file_part(const char *pathname);
unsigned long file_mod_time(const char *filename);
int64_t read_safely(const int fh, void * const buf, const int64_t len);
bool buffer_file_modified(const buffer *b, const char *name);
char *str_dup(const char *s);
int64_t strnlen_ne(const char *s, int64_t n);
int strcmpp(const void *a, const void *b);
int strdictcmpp(const void *a, const void *b);
int strdictcmp(const char *a, const char *b);
int filenamecmpp(const void *a, const void *b);
int filenamecmp(const char *a, const char *b);
void set_interactive_mode(void);
void unset_interactive_mode(void);
int64_t calc_width(const line_desc *ld, int64_t n, int tab_size, encoding_type encoding);
int64_t calc_char_len(const line_desc *ld, encoding_type encoding);
int64_t calc_pos(const line_desc *ld, int64_t n, int tab_size, encoding_type encoding);
int get_string_width(const char * const s, const int len, const encoding_type encoding);
int max_prefix(const char *s, const char *t);
bool is_prefix(const char *p, const char *s);
bool is_migrated(const char *name);
bool is_directory(const char *name);
bool is_ascii(const char *s, int len);
bool isparaspot(int c);
bool isasciispace(int c);
bool isasciipunct(int c);
bool isasciialpha(int c);
int asciitoupper(int c);
int asciitolower(int c);
encoding_type detect_encoding(const char *s, int64_t len);
int64_t next_pos(const char *s, int64_t pos, encoding_type encoding);
int64_t prev_pos(const char *s, int64_t pos, encoding_type encoding);
int get_char(const char *s, encoding_type encoding);
int get_char_width(const char * const s, const encoding_type encoding);
bool ne_ispunct(const int c, const int encoding);
bool ne_isspace(const int c, const int encoding);
bool ne_isword(const int c, const int encoding);
int context_prefix(const buffer *b, char **p, int64_t *prefix_pos);
line_desc *nth_line_desc(const buffer *b, const int n);

/* term.c */
int output_width(int c);
void ring_bell(void);
void do_flash(void);
void set_terminal_modes(void);
void reset_terminal_modes(void);
void set_terminal_window(int size);
void standout_on(void);
void standout_off(void);
void cursor_on(void);
void cursor_off(void);
void move_cursor(int row, int col);
void clear_end_of_line(int first_unused_hpos);
void clear_to_eol(void);
void clear_to_end(void);
void clear_entire_screen(void);
void set_attr(const unsigned int);
void output_chars(const char *string, const unsigned int *attr, int raw_len, int utf8);
void output_string(const char *s, int utf8);
void output_spaces(int n, const unsigned int *attr);
void output_char(int c, unsigned int attr, int utf8);
void insert_chars(const char *start, const unsigned int *attr, int raw_len, int utf8);
void insert_char(int c, const unsigned int attr, int utf8);
void delete_chars(int n);
int ins_del_lines(int vpos, int n);
int ttysize(void);
void term_init(void);

/* undo.c */
void start_undo_chain(buffer *b);
void end_undo_chain(buffer *b);
int add_undo_step(buffer *b, int64_t line, int64_t pos, int64_t len);
void fix_last_undo_step(buffer *b, int64_t delta);
int add_to_undo_stream(undo_buffer *ub, const char *p, int64_t len);
void reset_undo_buffer(undo_buffer *ub);
int undo(buffer *b);
int redo(buffer *b);
