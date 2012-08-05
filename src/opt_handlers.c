/* vifm
 * Copyright (C) 2011 xaizek.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <math.h> /* abs() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg/config.h"
#include "engine/options.h"
#include "modes/view.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "color_scheme.h"
#include "column_view.h"
#include "filelist.h"
#include "quickview.h"
#include "sort.h"
#include "status.h"
#include "ui.h"
#include "viewcolumns_parser.h"

#include "opt_handlers.h"

typedef union
{
	int *bool_val;
	int *int_val;
	char **str_val;
	int *enum_item;
	int *set_items;
}optvalref_t;

typedef void (*optinit_func)(optval_t *val);

typedef struct
{
	optvalref_t ref;
	optinit_func init;
}optinit_t;

static void init_columns(optval_t *val);
static void init_cpoptions(optval_t *val);
static void init_lines(optval_t *val);
static void init_timefmt(optval_t *val);
static void init_trash_dir(optval_t *val);
static void init_sort(optval_t *val);
static void init_sortorder(optval_t *val);
static void init_viewcolumns(optval_t *val);
static void load_options_defaults(void);
static void add_options(void);
static void print_func(const char *msg, const char *description);
static void autochpos_handler(OPT_OP op, optval_t val);
static void columns_handler(OPT_OP op, optval_t val);
static void confirm_handler(OPT_OP op, optval_t val);
static void cpoptions_handler(OPT_OP op, optval_t val);
static void fastrun_handler(OPT_OP op, optval_t val);
static void followlinks_handler(OPT_OP op, optval_t val);
static void fusehome_handler(OPT_OP op, optval_t val);
static void gdefault_handler(OPT_OP op, optval_t val);
static void history_handler(OPT_OP op, optval_t val);
static void hlsearch_handler(OPT_OP op, optval_t val);
static void iec_handler(OPT_OP op, optval_t val);
static void ignorecase_handler(OPT_OP op, optval_t val);
static void incsearch_handler(OPT_OP op, optval_t val);
static void laststatus_handler(OPT_OP op, optval_t val);
static void lines_handler(OPT_OP op, optval_t val);
static void scroll_line_down(FileView *view);
static void rulerformat_handler(OPT_OP op, optval_t val);
static void runexec_handler(OPT_OP op, optval_t val);
static void scrollbind_handler(OPT_OP op, optval_t val);
static void scrolloff_handler(OPT_OP op, optval_t val);
static void shell_handler(OPT_OP op, optval_t val);
#ifndef _WIN32
static void slowfs_handler(OPT_OP op, optval_t val);
#endif
static void smartcase_handler(OPT_OP op, optval_t val);
static void sortnumbers_handler(OPT_OP op, optval_t val);
static void sort_handler(OPT_OP op, optval_t val);
static void sortorder_handler(OPT_OP op, optval_t val);
static void viewcolumns_handler(OPT_OP op, optval_t val);
static void add_column(columns_t columns, column_info_t column_info);
static int map_name(const char *name);
static void resort_view(FileView * view);
static void statusline_handler(OPT_OP op, optval_t val);
static void tabstop_handler(OPT_OP op, optval_t val);
static void timefmt_handler(OPT_OP op, optval_t val);
static void timeoutlen_handler(OPT_OP op, optval_t val);
static void trash_handler(OPT_OP op, optval_t val);
static void trashdir_handler(OPT_OP op, optval_t val);
static void undolevels_handler(OPT_OP op, optval_t val);
static void vicmd_handler(OPT_OP op, optval_t val);
static void vixcmd_handler(OPT_OP op, optval_t val);
static void vifminfo_handler(OPT_OP op, optval_t val);
static void vimhelp_handler(OPT_OP op, optval_t val);
static void wildmenu_handler(OPT_OP op, optval_t val);
static void wrap_handler(OPT_OP op, optval_t val);
static void wrapscan_handler(OPT_OP op, optval_t val);

static int save_msg;
static char print_buf[320*80];

static const char * sort_enum[] = {
	"ext",
	"name",
#ifndef _WIN32
	"gid",
	"gname",
	"mode",
	"uid",
	"uname",
#endif
	"size",
	"atime",
	"ctime",
	"mtime",
	"iname",
};
ARRAY_GUARD(sort_enum, NUM_SORT_OPTIONS);

static const char * sort_types[] = {
	"ext",   "+ext",   "-ext",
	"name",  "+name",  "-name",
#ifndef _WIN32
	"gid",   "+gid",   "-gid",
	"gname", "+gname", "-gname",
	"mode",  "+mode",  "-mode",
	"uid",   "+uid",   "-uid",
	"uname", "+uname", "-uname",
#endif
	"size",  "+size",  "-size",
	"atime", "+atime", "-atime",
	"ctime", "+ctime", "-ctime",
	"mtime", "+mtime", "-mtime",
	"iname", "+iname", "-iname",
};
ARRAY_GUARD(sort_types, NUM_SORT_OPTIONS*3);

static const char * sortorder_enum[] = {
	"ascending",
	"descending",
};

static const char * vifminfo_set[] = {
	"options",
	"filetypes",
	"commands",
	"bookmarks",
	"tui",
	"dhistory",
	"state",
	"cs",
	"savedirs",
	"chistory",
	"shistory",
	"dirstack",
	"registers",
	"phistory",
};

static struct
{
	const char *name;
	const char *abbr;
	OPT_TYPE type;
	int val_count;
	const char **vals;
	opt_handler handler;
	optinit_t initializer;
	optval_t val;
}options[] = {
	/* global options */
	{ "autochpos",   "",     OPT_BOOL,    0,                          NULL,            &autochpos_handler,
		{ .ref.bool_val = &cfg.auto_ch_pos }                                                                   },
	{ "columns",     "co",   OPT_INT,     0,                          NULL,            &columns_handler,
		{ .init = &init_columns }                                                                              },
	{ "confirm",     "cf",   OPT_BOOL,    0,                          NULL,            &confirm_handler,
		{ .ref.bool_val = &cfg.confirm }                                                                       },
	{ "cpoptions",   "cpo",  OPT_STR,     0,                          NULL,            &cpoptions_handler,
		{ .init = &init_cpoptions }                                                                            },
	{ "fastrun",     "",     OPT_BOOL,    0,                          NULL,            &fastrun_handler,
		{ .ref.bool_val = &cfg.fast_run }                                                                      },
	{ "followlinks", "",     OPT_BOOL,    0,                          NULL,            &followlinks_handler,
		{ .ref.bool_val = &cfg.follow_links }                                                                  },
	{ "fusehome",    "",     OPT_STR,     0,                          NULL,            &fusehome_handler,
		{ .ref.str_val = &cfg.fuse_home }                                                                      },
	{ "gdefault",    "gd",   OPT_BOOL,    0,                          NULL,            &gdefault_handler,
		{ .ref.bool_val = &cfg.gdefault }                                                                      },
	{ "history",     "hi",   OPT_INT,     0,                          NULL,            &history_handler,
		{ .ref.int_val = &cfg.history_len }                                                                    },
	{ "hlsearch",    "hls",  OPT_BOOL,    0,                          NULL,            &hlsearch_handler,
		{ .ref.bool_val = &cfg.hl_search }                                                                     },
	{ "iec",         "",     OPT_BOOL,    0,                          NULL,            &iec_handler,
		{ .ref.bool_val = &cfg.use_iec_prefixes }                                                              },
	{ "ignorecase",  "ic",   OPT_BOOL,    0,                          NULL,            &ignorecase_handler,
		{ .ref.bool_val = &cfg.ignore_case }                                                                   },
	{ "incsearch",   "is",   OPT_BOOL,    0,                          NULL,            &incsearch_handler ,
		{ .ref.bool_val = &cfg.inc_search }                                                                    },
	{ "laststatus",  "ls",   OPT_BOOL,    0,                          NULL,            &laststatus_handler,
		{ .ref.bool_val = &cfg.last_status }                                                                   },
	{ "lines",       "",     OPT_INT,     0,                          NULL,            &lines_handler,
		{ .init = &init_lines }                                                                                },
	{ "rulerformat", "ruf",  OPT_STR,     0,                          NULL,            &rulerformat_handler,
		{ .ref.str_val = &cfg.ruler_format }                                                                    },
	{ "runexec",     "",     OPT_BOOL,    0,                          NULL,            &runexec_handler,
		{ .ref.bool_val = &cfg.auto_execute }                                                                  },
	{ "scrollbind",  "scb",  OPT_BOOL,    0,                          NULL,            &scrollbind_handler,
		{ .ref.bool_val = &cfg.scroll_bind }                                                                   },
	{ "scrolloff",   "so",   OPT_INT,     0,                          NULL,            &scrolloff_handler,
		{ .ref.int_val = &cfg.scroll_off }                                                                     },
	{ "shell",       "sh",   OPT_STR,     0,                          NULL,            &shell_handler,
		{ .ref.str_val = &cfg.shell }                                                                          },
#ifndef _WIN32
	{ "slowfs",      "",     OPT_STRLIST, 0,                          NULL,            &slowfs_handler,
		{ .ref.str_val = &cfg.slow_fs_list }                                                                   },
#endif
	{ "smartcase",   "scs",  OPT_BOOL,    0,                          NULL,            &smartcase_handler,
		{ .ref.bool_val = &cfg.smart_case }                                                                    },
	{ "sortnumbers", "",     OPT_BOOL,    0,                          NULL,            &sortnumbers_handler,
		{ .ref.bool_val = &cfg.sort_numbers }                                                                  },
	{ "statusline",  "stl",  OPT_STR,     0,                          NULL,            &statusline_handler,
		{ .ref.str_val = &cfg.status_line }                                                                    },
	{ "tabstop",     "ts",   OPT_INT,     0,                          NULL,            &tabstop_handler,
		{ .ref.int_val = &cfg.tab_stop }                                                                       },
	{ "timefmt",     "",     OPT_STR,     0,                          NULL,            &timefmt_handler,
		{ .init = &init_timefmt }                                                                              },
	{ "timeoutlen",  "tm",   OPT_INT,     0,                          NULL,            &timeoutlen_handler,
		{ .ref.int_val = &cfg.timeout_len }                                                                    },
	{ "trash",       "",     OPT_BOOL,    0,                          NULL,            &trash_handler,
		{ .ref.bool_val = &cfg.use_trash }                                                                     },
	{ "trashdir",    "",     OPT_STR,     0,                          NULL,            &trashdir_handler,
		{ .init = &init_trash_dir }                                                                            },
	{ "undolevels",  "ul",   OPT_INT,     0,                          NULL,            &undolevels_handler,
		{ .ref.int_val = &cfg.undo_levels }                                                                    },
	{ "vicmd",       "",     OPT_STR,     0,                          NULL,            &vicmd_handler,
		{ .ref.str_val = &cfg.vi_command }                                                                     },
	{ "vixcmd",      "",     OPT_STR,     0,                          NULL,            &vixcmd_handler,
		{ .ref.str_val = &cfg.vi_x_command }                                                                   },
	{ "vifminfo",    "",     OPT_SET,     ARRAY_LEN(vifminfo_set),    vifminfo_set,    &vifminfo_handler,
		{ .ref.set_items = &cfg.vifm_info }                                                                    },
	{ "vimhelp",     "",     OPT_BOOL,    0,                          NULL,            &vimhelp_handler,
		{ .ref.bool_val = &cfg.use_vim_help }                                                                  },
	{ "wildmenu",    "wmnu", OPT_BOOL,    0,                          NULL,            &wildmenu_handler,
		{ .ref.bool_val = &cfg.wild_menu }                                                                     },
	{ "wrap",        "",     OPT_BOOL,    0,                          NULL,            &wrap_handler,
		{ .ref.bool_val = &cfg.wrap_quick_view }                                                               },
	{ "wrapscan",    "ws",   OPT_BOOL,    0,                          NULL,            &wrapscan_handler,
		{ .ref.bool_val = &cfg.wrap_scan }                                                                     },
	/* local options */
	{ "sort",        "",     OPT_STRLIST, ARRAY_LEN(sort_types),      sort_types,      &sort_handler,
		{ .init = &init_sort }                                                                                 },
	{ "sortorder",   "",     OPT_ENUM,    ARRAY_LEN(sortorder_enum),  sortorder_enum,  &sortorder_handler,
		{ .init = &init_sortorder }                                                                            },
	{ "viewcolumns", "",     OPT_STRLIST, 0,                          NULL,            &viewcolumns_handler,
		{ .init = &init_viewcolumns }                                                                          },
};

void
init_option_handlers(void)
{
	static int opt_changed;
	init_options(&opt_changed, &print_func);
	load_options_defaults();
	add_options();
}

static void
init_columns(optval_t *val)
{
	val->int_val = (cfg.columns > 0) ? cfg.columns : getmaxx(stdscr);
}

static void
init_cpoptions(optval_t *val)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%s", cfg.selection_cp ? "s" : "");
	val->str_val = buf;
}

static void
init_lines(optval_t *val)
{
	val->int_val = (cfg.lines > 0) ? cfg.lines : getmaxy(stdscr);
}

static void
init_timefmt(optval_t *val)
{
	val->str_val = cfg.time_format + 1;
}

static void
init_trash_dir(optval_t *val)
{
	val->str_val = cfg.trash_dir;
}

static void
init_sort(optval_t *val)
{
	val->str_val = "+name";
}

static void
init_sortorder(optval_t *val)
{
	val->enum_item = 0;
}

static void
init_viewcolumns(optval_t *val)
{
	val->str_val = "";
}

static void
load_options_defaults(void)
{
	int i;
	for(i = 0; i < ARRAY_LEN(options); i++)
	{
		if(options[i].initializer.init != NULL)
		{
			options[i].initializer.init(&options[i].val);
		}
		else
		{
			options[i].val.str_val = *options[i].initializer.ref.str_val;
		}
	}
}

static void
add_options(void)
{
	int i;
	for(i = 0; i < ARRAY_LEN(options); i++)
	{
		add_option(options[i].name, options[i].abbr, options[i].type,
				options[i].val_count, options[i].vals, options[i].handler,
				options[i].val);
	}
}

void
load_local_options(FileView *view)
{
	optval_t val;

	load_sort_option(view);

	val.str_val = view->view_columns;
	set_option("viewcolumns", val);
}

void
load_sort_option(FileView *view)
{
	optval_t val;
	char buf[64] = "";
	int j, i;

	for(j = 0; j < NUM_SORT_OPTIONS && view->sort[j] <= NUM_SORT_OPTIONS; j++)
		if(abs(view->sort[j]) == SORT_BY_NAME ||
				abs(view->sort[j]) == SORT_BY_INAME)
			break;
	if(j < NUM_SORT_OPTIONS && view->sort[j] > NUM_SORT_OPTIONS)
#ifndef _WIN32
		view->sort[j++] = SORT_BY_NAME;
#else
		view->sort[j++] = SORT_BY_INAME;
#endif

	i = -1;
	while(++i < NUM_SORT_OPTIONS && view->sort[i] <= NUM_SORT_OPTIONS)
	{
		if(buf[0] != '\0')
			strcat(buf, ",");
		if(view->sort[i] < 0)
			strcat(buf, "-");
		else
			strcat(buf, "+");
		strcat(buf, sort_enum[abs(view->sort[i]) - 1]);
	}

	val.str_val = buf;
	set_option("sort", val);

	val.enum_item = (view->sort[0] < 0);
	set_option("sortorder", val);
}

int
process_set_args(const char *args)
{
	save_msg = 0;
	print_buf[0] = '\0';
	if(set_options(args) != 0) /* changes print_buf and save_msg */
	{
		print_func("", "Invalid argument for :set command");
		status_bar_error(print_buf);
		save_msg = -1;
	}
	else if(print_buf[0] != '\0')
	{
		status_bar_message(print_buf);
	}
	return save_msg;
}

static void
print_func(const char *msg, const char *description)
{
	if(print_buf[0] != '\0')
	{
		strncat(print_buf, "\n", sizeof(print_buf) - strlen(print_buf) - 1);
	}
	if(*msg == '\0')
	{
		strncat(print_buf, description, sizeof(print_buf) - strlen(print_buf) - 1);
	}
	else
	{
		snprintf(print_buf, sizeof(print_buf) - strlen(print_buf), "%s: %s", msg,
				description);
	}
	save_msg = 1;
}

static void
autochpos_handler(OPT_OP op, optval_t val)
{
	cfg.auto_ch_pos = val.bool_val;
	if(curr_stats.load_stage < 2)
	{
		clean_positions_in_history(curr_view);
		clean_positions_in_history(other_view);
	}
}

static void
columns_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < MIN_TERM_WIDTH)
	{
		char buf[128];
		val.int_val = MIN_TERM_WIDTH;
		snprintf(buf, sizeof(buf), "At least %d columns needed", MIN_TERM_WIDTH);
		print_func("", buf);
	}

	if(cfg.columns != val.int_val)
	{
		resize_term(getmaxy(stdscr), val.int_val);
		update_screen(UT_FULL);
		cfg.columns = getmaxx(stdscr);
	}

	val.int_val = cfg.columns;
	set_option("columns", val);
}

static void
confirm_handler(OPT_OP op, optval_t val)
{
	cfg.confirm = val.bool_val;
}

static void
cpoptions_handler(OPT_OP op, optval_t val)
{
	const char VALID[] = "s";
	char buf[ARRAY_LEN(VALID)];
	char *p;

	buf[0] = '\0';
	p = val.str_val;
	while(*p != '\0')
	{
		if(char_is_one_of(VALID, *p))
		{
			buf[strlen(buf) + 1] = '\0';
			buf[strlen(buf)] = *p;
		}
		p++;
	}

	if(strcmp(val.str_val, buf) != 0)
	{
		val.str_val = buf;
		set_option("cpoptions", val);
		return;
	}

	cfg.selection_cp = 0;

	p = buf;
	while(*p != '\0')
	{
		if(*p == 's')
			cfg.selection_cp = 1;
		p++;
	}
}

static void
fastrun_handler(OPT_OP op, optval_t val)
{
	cfg.fast_run = val.bool_val;
}

static void
followlinks_handler(OPT_OP op, optval_t val)
{
	cfg.follow_links = val.bool_val;
}

static void
fusehome_handler(OPT_OP op, optval_t val)
{
	free(cfg.fuse_home);
	cfg.fuse_home = expand_tilde(strdup(val.str_val));
}

static void
gdefault_handler(OPT_OP op, optval_t val)
{
	cfg.gdefault = val.bool_val;
}

static void
history_handler(OPT_OP op, optval_t val)
{
	resize_history(MAX(0, val.int_val));
}

static void
hlsearch_handler(OPT_OP op, optval_t val)
{
	cfg.hl_search = val.bool_val;
}

static void
iec_handler(OPT_OP op, optval_t val)
{
	cfg.use_iec_prefixes = val.bool_val;

	redraw_lists();
}

static void
ignorecase_handler(OPT_OP op, optval_t val)
{
	cfg.ignore_case = val.bool_val;
}

static void
incsearch_handler(OPT_OP op, optval_t val)
{
	cfg.inc_search = val.bool_val;
}

static void
laststatus_handler(OPT_OP op, optval_t val)
{
	cfg.last_status = val.bool_val;
	if(cfg.last_status)
	{
		if(curr_stats.split == VSPLIT)
			scroll_line_down(&lwin);
		scroll_line_down(&rwin);
	}
	else
	{
		if(curr_stats.split == VSPLIT)
			lwin.window_rows++;
		rwin.window_rows++;
		wresize(lwin.win, lwin.window_rows + 1, lwin.window_width + 1);
		wresize(rwin.win, rwin.window_rows + 1, rwin.window_width + 1);
		draw_dir_list(&lwin, lwin.top_line);
		draw_dir_list(&rwin, rwin.top_line);
	}
	move_to_list_pos(curr_view, curr_view->list_pos);
	touchwin(lwin.win);
	touchwin(rwin.win);
	touchwin(lborder);
	if(curr_stats.split == VSPLIT)
		touchwin(mborder);
	touchwin(rborder);
	wnoutrefresh(lwin.win);
	wnoutrefresh(rwin.win);
	wnoutrefresh(lborder);
	wnoutrefresh(mborder);
	wnoutrefresh(rborder);
	doupdate();
}

static void
lines_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < MIN_TERM_HEIGHT)
	{
		char buf[128];
		val.int_val = MIN_TERM_HEIGHT;
		snprintf(buf, sizeof(buf), "At least %d lines needed", MIN_TERM_HEIGHT);
		print_func("", buf);
	}

	if(cfg.lines != val.int_val)
	{
		LOG_INFO_MSG("resize_term(%d, %d)", val.int_val, getmaxx(stdscr));
		resize_term(val.int_val, getmaxx(stdscr));
		update_screen(UT_FULL);
		cfg.lines = getmaxy(stdscr);
	}

	val.int_val = cfg.lines;
	set_option("lines", val);
}

static void
scroll_line_down(FileView *view)
{
	view->window_rows--;
	if(view->list_pos == view->top_line + view->window_rows + 1)
	{
		view->top_line++;
		view->curr_line--;
		draw_dir_list(view, view->top_line);
	}
	wresize(view->win, view->window_rows + 1, view->window_width + 1);
}

static void
rulerformat_handler(OPT_OP op, optval_t val)
{
	char *s;
	if((s = strdup(val.str_val)) == NULL)
		return;
	free(cfg.ruler_format);
	cfg.ruler_format = s;
}

static void
runexec_handler(OPT_OP op, optval_t val)
{
	cfg.auto_execute = val.bool_val;
}

static void
scrollbind_handler(OPT_OP op, optval_t val)
{
	cfg.scroll_bind = val.bool_val;
	curr_stats.scroll_bind_off = rwin.top_line - lwin.top_line;
}

static void
scrolloff_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < 0)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Invalid scroll size: %d", val.int_val);
		print_func("", buf);
		reset_option_to_default("scrolloff");
		return;
	}

	cfg.scroll_off = val.int_val;
	if(cfg.scroll_off > 0)
		move_to_list_pos(curr_view, curr_view->list_pos);
}

static void
shell_handler(OPT_OP op, optval_t val)
{
	char *s;
	if((s = strdup(val.str_val)) == NULL)
		return;
	free(cfg.shell);
	cfg.shell = s;
}

#ifndef _WIN32
static void
slowfs_handler(OPT_OP op, optval_t val)
{
	char *s;
	if((s = strdup(val.str_val)) == NULL)
		return;
	free(cfg.slow_fs_list);
	cfg.slow_fs_list = s;
}
#endif

static void
smartcase_handler(OPT_OP op, optval_t val)
{
	cfg.smart_case = val.bool_val;
}

static void
sortnumbers_handler(OPT_OP op, optval_t val)
{
	cfg.sort_numbers = val.bool_val;
	resort_dir_list(1, curr_view);
	resort_dir_list(1, other_view);
	redraw_lists();
}

static void
sort_handler(OPT_OP op, optval_t val)
{
	char *p = val.str_val - 1;
	int i, j;

	i = 0;
	do
	{
		char buf[32];
		char *t;
		int minus;
		int pos;

		t = p + 1;
		p = strchr(t, ',');
		if(p == NULL)
			p = t + strlen(t);

		minus = (*t == '-');
		if(*t == '-' || *t == '+')
			t++;

		snprintf(buf, MIN(p - t + 1, sizeof(buf)), "%s", t);

		if((pos = string_array_pos((char **)sort_enum, ARRAY_LEN(sort_enum),
				buf)) == -1)
			continue;
		pos++;

		for(j = 0; j < i; j++)
			if(abs(curr_view->sort[j]) == pos)
				break;

		if(minus)
			curr_view->sort[j] = -pos;
		else
			curr_view->sort[j] = pos;
		if(j == i)
			i++;
	}
	while(*p != '\0');

	for(j = 0; j < i; j++)
	{
		int sort_key = abs(curr_view->sort[j]);
		if(sort_key == SORT_BY_NAME || sort_key == SORT_BY_INAME)
			break;
	}
	if(j == i)
#ifndef _WIN32
		curr_view->sort[i++] = SORT_BY_NAME;
#else
		curr_view->sort[i++] = SORT_BY_INAME;
#endif
	while(i < NUM_SORT_OPTIONS)
		curr_view->sort[i++] = NUM_SORT_OPTIONS + 1;

	resort_view(curr_view);
	move_to_list_pos(curr_view, curr_view->list_pos);
	load_sort_option(curr_view);
}

static void
sortorder_handler(OPT_OP op, optval_t val)
{
	if((val.enum_item ? -1 : +1)*curr_view->sort[0] < 0)
	{
		curr_view->sort[0] = -curr_view->sort[0];

		resort_view(curr_view);
		load_sort_option(curr_view);
	}
}

static void
viewcolumns_handler(OPT_OP op, optval_t val)
{
	columns_clear(curr_view->columns);
	if(val.str_val[0] == '\0')
	{
		char buffer[128];
		(void)snprintf(buffer, sizeof(buffer), "-{name},{%s}",
				sort_enum[get_secondary_key(abs(curr_view->sort[0])) - 1]);
		(void)parse_columns(curr_view->columns, add_column, map_name,
				buffer);
		replace_string(&curr_view->view_columns, "");
		redraw_current_view();
		return;
	}

	if(parse_columns(curr_view->columns, add_column, map_name, val.str_val) != 0)
	{
		print_func("", "Invalid format of 'viewcolumns' option");
		(void)parse_columns(curr_view->columns, add_column, map_name,
				curr_view->view_columns);
	}
	else
	{
		replace_string(&curr_view->view_columns, val.str_val);
		redraw_current_view();
	}
}

/* Adds new column to view columns. */
static void
add_column(columns_t columns, column_info_t column_info)
{
	columns_add_column(columns, column_info);
}

/* Maps column name to column id. Returns column id. */
static int
map_name(const char *name)
{
	int pos;
	pos = string_array_pos((char **)sort_enum, ARRAY_LEN(sort_enum), name);
	/* Position is sort key minus one. */
	return pos + 1;
}

static void
resort_view(FileView * view)
{
	resort_dir_list(1, view);
	draw_dir_list(view, view->top_line);
	refresh_view_win(view);
}

static void
statusline_handler(OPT_OP op, optval_t val)
{
	char *s;
	if((s = strdup(val.str_val)) == NULL)
		return;
	free(cfg.status_line);
	cfg.status_line = s;
}

static void
tabstop_handler(OPT_OP op, optval_t val)
{
	if(val.int_val <= 0)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Argument must be positive: %d", val.int_val);
		print_func("", buf);
		reset_option_to_default("tabstop");
		return;
	}

	cfg.tab_stop = val.int_val;
	if(curr_stats.view)
		quick_view_file(curr_view);
	else if(other_view->explore_mode)
		view_redraw();
}

static void
timefmt_handler(OPT_OP op, optval_t val)
{
	free(cfg.time_format);
	cfg.time_format = malloc(1 + strlen(val.str_val) + 1);
	strcpy(cfg.time_format, " ");
	strcat(cfg.time_format, val.str_val);

	redraw_lists();
}

static void
timeoutlen_handler(OPT_OP op, optval_t val)
{
	if(val.int_val < 0)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Argument must be >= 0: %d", val.int_val);
		print_func("", buf);
		val.int_val = 0;
		set_option("timeoutlen", val);
		return;
	}

	cfg.timeout_len = val.int_val;
}

static void
trash_handler(OPT_OP op, optval_t val)
{
	cfg.use_trash = val.bool_val;
}

static void
trashdir_handler(OPT_OP op, optval_t val)
{
	char *s = expand_tilde(strdup(val.str_val));
	snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), "%s", s);
	create_trash_dir();
	free(s);
}

static void
undolevels_handler(OPT_OP op, optval_t val)
{
	cfg.undo_levels = val.int_val;
}

static void
vicmd_handler(OPT_OP op, optval_t val)
{
	replace_string(&cfg.vi_command, val.str_val);
	cfg.vi_cmd_bg = ends_with(cfg.vi_command, "&");
	if(cfg.vi_cmd_bg)
		cfg.vi_command[strlen(cfg.vi_command) - 1] = '\0';
}

static void
vixcmd_handler(OPT_OP op, optval_t val)
{
	replace_string(&cfg.vi_x_command, val.str_val);
	cfg.vi_x_cmd_bg = ends_with(cfg.vi_x_command, "&");
	if(cfg.vi_x_cmd_bg)
		cfg.vi_x_command[strlen(cfg.vi_x_command) - 1] = '\0';
}

static void
vifminfo_handler(OPT_OP op, optval_t val)
{
	cfg.vifm_info = val.set_items;
}

static void
vimhelp_handler(OPT_OP op, optval_t val)
{
	cfg.use_vim_help = val.bool_val;
}

static void
wildmenu_handler(OPT_OP op, optval_t val)
{
	cfg.wild_menu = val.bool_val;
}

static void
wrap_handler(OPT_OP op, optval_t val)
{
	cfg.wrap_quick_view = val.bool_val;
	if(curr_stats.view)
		quick_view_file(curr_view);
}

static void
wrapscan_handler(OPT_OP op, optval_t val)
{
	cfg.wrap_scan = val.bool_val;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
