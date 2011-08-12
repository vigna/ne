/* Function prototypes

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2011 Todd M. Lewis and Sebastiano Vigna

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
int do_action(buffer *b, action a, int c, unsigned char *p);

/* autocomp.c */
unsigned char *autocomplete(unsigned char *p, char *req_msg, const int ext, int * const error);

/* buffer.c */
encoding_type detect_buffer_encoding(const buffer *b);
char_pool *alloc_char_pool(int size);
void free_char_pool(char_pool *cp);
char_pool *get_char_pool(buffer *b, unsigned char *p);
line_desc_pool *alloc_line_desc_pool(int pool_size);
void free_line_desc_pool(line_desc_pool *ldp);
buffer *alloc_buffer(const buffer *cur_b);
void free_buffer_contents(buffer *b);
void clear_buffer(buffer *b);
void free_buffer(buffer *b);
int calc_lost_chars(const buffer *b);
buffer *get_nth_buffer(int n);
buffer *get_buffer_named(const char *p);
int modified_buffers(void);
int save_all_modified_buffers(void);
line_desc *alloc_line_desc(buffer *b);
void free_line_desc(buffer *b, line_desc *ld);
unsigned char *alloc_chars(buffer *b, int len);
int alloc_chars_around(buffer *b, line_desc *ld, int n, int check_first_before);
void free_chars(buffer *b, unsigned char *p, int len);
int insert_one_line(buffer *b, line_desc *ld, int line, int pos);
int delete_one_line(buffer *b, line_desc *ld, int line);
int undelete_line(buffer *b);
void delete_to_eol(buffer *b, line_desc *ld, int line, int pos);
int insert_stream(buffer *b, line_desc *ld, int line, int pos, const unsigned char *stream, int stream_len);
int insert_one_char(buffer *b, line_desc *ld, int line, int pos, int c);
int insert_spaces(buffer *b, line_desc *ld, int line, int pos, int n);
int delete_stream(buffer *b, line_desc *ld, int line, int pos, int len);
int delete_one_char(buffer *b, line_desc *ld, int line, int pos);
void change_filename(buffer *b, char *name);
void ensure_attr_buf(buffer * const b, const int capacity);
int load_file_in_buffer(buffer *b, const char *name);
int load_fh_in_buffer(buffer *b, int fh);
int save_buffer_to_file(buffer *b, const char *name);
void auto_save(buffer *b);
void reset_syntax_states(buffer *b);

/* clips.c */
clip_desc *alloc_clip_desc(int n, int size);
clip_desc *realloc_clip_desc(clip_desc *cd, int n, int size);
void free_clip_desc(clip_desc *cd);
int is_encoding_neutral(clip_desc *cd);
clip_desc *get_nth_clip(int n);
int copy_to_clip(buffer *b, int n, int cut);
int erase_block(buffer *b);
int paste_to_buffer(buffer *b, int n);
int copy_vert_to_clip(buffer *b, int n, int cut);
int erase_vert_block(buffer *b);
int paste_vert_to_buffer(buffer *b, int n);
int load_clip(int n, const char *name, int preserve_cr, int binary);
int save_clip(int n, const char *name, int CRLF, int binary);

/* command.c */
void build_hash_table(void);
void build_command_name_table(void);
action parse_command_line(const unsigned char *command_line, int *num_arg, unsigned char **string_arg, int exec_only_options);
int execute_command_line(buffer *b, const unsigned char *command_line);
macro_desc *alloc_macro_desc(void);
void free_macro_desc(macro_desc *md);
void record_action(char_stream *cs, action a, int c, unsigned char *p, int verbose);
int play_macro(buffer *b, char_stream *cs);
macro_desc *load_macro(const char *name);
int execute_macro(buffer *b, const char *name);
void help(char *p);
int cmdcmp(const unsigned char *c, const unsigned char *m);
void unload_macros(void);

void optimize_macro(char_stream *cs, int verbose);

/* display.c */
void update_syntax_states(buffer *b, int row, line_desc *ld, line_desc *end_ld);
int highlight_cmp(HIGHLIGHT_STATE *x, HIGHLIGHT_STATE *y);
void delay_update();
void output_line_desc(int row, int col, line_desc *ld, int start, int len, int tab_size, int cleared_at_end, int utf8, const int * const attr, const int * const diff, const int diff_size);
line_desc *update_partial_line(buffer *b, int n, int start_x, int cleared_at_end, const int differential);
void update_line(buffer *b, int n, const int cleared_at_end, const int differential);
void update_window_lines(buffer *b, int start_line, int end_line, int doit);
void update_syntax_and_lines(buffer *b, line_desc *start_ld, line_desc *end_ld);
void update_window(buffer *b);
void update_deleted_char(buffer *b, int c, int a, const line_desc *ld, int pos, int attr_pos, int line, int x);
void update_inserted_char(buffer *b, int c, const line_desc *ld, int pos, int attr_pos, int line, int x);
void update_overwritten_char(buffer *b, int old_char, int new_char, const line_desc *ld, int pos, int attr_pos, int line, int x);
void reset_window(void);
void refresh_window(buffer *b);
void scroll_window(buffer *b, int line, int n);
HIGHLIGHT_STATE freeze_attributes(buffer *b, line_desc *ld);
void automatch_bracket(buffer * const b, const int show);

/* edit.c */
int to_upper(buffer *b);
int to_lower(buffer *b);
int capitalize(buffer *b);
int match_bracket(buffer *b);
int find_matching_bracket(buffer *b, const int min_line, const int max_line, int *match_line, int *match_pos, int *c, line_desc ** ld);
int word_wrap(buffer *b);
int paragraph(buffer *b);
int center(buffer *b);
int auto_indent_line(buffer * const b, const int line, line_desc * const ld, const int up_to_col);
int backtab(buffer *b);

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
int key_may_set(const char * const cap_string, const int code);

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
int  adjust_view(buffer *b, const unsigned char *p);
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
void goto_column(buffer *b, int n);
void goto_line(buffer *b, int n);
void goto_pos(buffer *b, int pos);
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
void about(int show);
void automatch_bracket(buffer *b, int show);
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
int   request_response(const buffer *b, const char *prompt, int default_value);
char  request_char(const buffer *b, const char *prompt, const char default_value);
int   request_number(const char *prompt, int default_value);
char *request_string(const char *prompt, const char *default_string, int accept_null_string, int completion_allowed, int prefer_utf8);
char *request(const char *prompt, const char *default_string, int alpha_allowed, int completion_allowed, int prefer_utf8);

/* request.c */
int   request_strings(const char * const * const entries, int num_entries, int default_entry, int max_name_len, int mark_char);
char *request_files(const char *filename, int use_prefix);
char *request_file(const buffer *b, const char *prompt, const char *default_name);
int   request_document(void);

/* search.c */
int  find(buffer *b, const unsigned char *pattern, const skip_first);
int  replace(buffer *b, int n, const char *string);
int  find_regexp(buffer *b, const unsigned char *regex, const int skip_first);
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
char_stream *alloc_char_stream(int size);
void free_char_stream(char_stream *cs);
char_stream *realloc_char_stream(char_stream *cs, int size);
int add_to_stream(char_stream *cs, const unsigned char *s, int len);
char_stream *reset_stream(char_stream *cs);
void set_stream_encoding(char_stream *cs, encoding_type source);
char_stream *load_stream(char_stream *cs, const char *name, int preserve_cr, int binary);
char_stream *load_stream_from_fh(char_stream *cs, int fh, int preserve_cr, int binary);
int save_stream(const char_stream *cs, const char *name, int CRLF, int binary);
int save_stream_to_fh(const char_stream *cs, int fh, int CRLF, int binary);

int delete_from_stream(char_stream *cs, int p, int len);
int insert_in_stream(char_stream *cs, const char *s, int p, int len);

/* support.c */
int same_str(const char *p, const char *q);
char *ne_getcwd(const int bufsize);
const char *get_global_dir(void);
const char *tilde_expand(const char *filename);
const char *file_part(const char *pathname);
char *str_dup(const char *s);
int strnlen_ne(const char *s, int n);
int strcmpp(const void *a, const void *b);
int strdictcmp(const void *a, const void *b);
int filenamecmpp(const void *a, const void *b);
void set_interactive_mode(void);
void unset_interactive_mode(void);
int calc_width(const line_desc *ld, int n, int tab_size, encoding_type encoding);
int calc_char_len(const line_desc *ld, encoding_type encoding);
int calc_pos(const line_desc *ld, int n, int tab_size, encoding_type encoding);
int get_string_width(const unsigned char * const s, const int len, const encoding_type encoding);
int max_prefix(const char *s, const char *t);
int is_prefix(const char *p, const char *s);
char *complete(const char *start_prefix);
int is_migrated(const char *name);
int is_directory(const char *name);
int is_ascii(const unsigned char *s, int len);
int isasciispace(int c);
int isasciipunct(int c);
int isasciialpha(int c);
int asciitoupper(int c);
int asciitolower(int c);
encoding_type detect_encoding(const unsigned char *s, int len);
int next_pos(const unsigned char *s, int pos, encoding_type encoding);
int prev_pos(const unsigned char *s, int pos, encoding_type encoding);
int get_char(const unsigned char *s, encoding_type encoding);
int get_char_width(const unsigned char * const s, const encoding_type encoding);
int ne_ispunct(const int c, const int encoding);
int ne_isspace(const int c, const int encoding);
int ne_isword(const int c, const int encoding);

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
void output_chars(const unsigned char *string, const unsigned int *attr, int raw_len, int utf8);
void output_string(const char *s, int utf8);
void output_spaces(int n, const unsigned int *attr);
void output_char(int c, unsigned int attr, int utf8);
void insert_chars(const unsigned char *start, const unsigned int *attr, int raw_len, int utf8);
void insert_char(int c, const unsigned int attr, int utf8);
void delete_chars(int n);
int ins_del_lines(int vpos, int n);
int ttysize(void);
void term_init(void);

/* undo.c */
void start_undo_chain(buffer *b);
void end_undo_chain(buffer *b);
int add_undo_step(buffer *b, int line, int pos, int len);
int add_to_undo_stream(undo_buffer *ub, const char *p, int len);
void reset_undo_buffer(undo_buffer *ub);
int undo(buffer *b);
int redo(buffer *b);
