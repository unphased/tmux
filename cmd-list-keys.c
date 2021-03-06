/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * List key bindings.
 */

static enum cmd_retval	cmd_list_keys_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval	cmd_list_keys_commands(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_list_keys_entry = {
	.name = "list-keys",
	.alias = "lsk",

	.args = { "T:", 0, 0 },
	.usage = "[-T key-table]",

	.flags = CMD_STARTSERVER|CMD_AFTERHOOK,
	.exec = cmd_list_keys_exec
};

const struct cmd_entry cmd_list_commands_entry = {
	.name = "list-commands",
	.alias = "lscm",

	.args = { "F:", 0, 0 },
	.usage = "[-F format]",

	.flags = CMD_STARTSERVER|CMD_AFTERHOOK,
	.exec = cmd_list_keys_exec
};

static enum cmd_retval
cmd_list_keys_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct key_table	*table;
	struct key_binding	*bd;
	const char		*tablename, *r;
	char			*key, *cp, *tmp;
	int			 repeat, width, tablewidth, keywidth;
	size_t			 tmpsize, tmpused, cplen;

	if (self->entry == &cmd_list_commands_entry)
		return (cmd_list_keys_commands(self, item));

	tablename = args_get(args, 'T');
	if (tablename != NULL && key_bindings_get_table(tablename, 0) == NULL) {
		cmdq_error(item, "table %s doesn't exist", tablename);
		return (CMD_RETURN_ERROR);
	}

	repeat = 0;
	tablewidth = keywidth = 0;
	table = key_bindings_first_table ();
	while (table != NULL) {
		if (tablename != NULL && strcmp(table->name, tablename) != 0) {
			table = key_bindings_next_table(table);
			continue;
		}
		bd = key_bindings_first(table);
		while (bd != NULL) {
			key = args_escape(key_string_lookup_key(bd->key));

			if (bd->flags & KEY_BINDING_REPEAT)
				repeat = 1;

			width = utf8_cstrwidth(table->name);
			if (width > tablewidth)
				tablewidth = width;
			width = utf8_cstrwidth(key);
			if (width > keywidth)
				keywidth = width;

			free(key);
			bd = key_bindings_next(table, bd);
		}
		table = key_bindings_next_table(table);
	}

	tmpsize = 256;
	tmp = xmalloc(tmpsize);

	table = key_bindings_first_table ();
	while (table != NULL) {
		if (tablename != NULL && strcmp(table->name, tablename) != 0) {
			table = key_bindings_next_table(table);
			continue;
		}
		bd = key_bindings_first(table);
		while (bd != NULL) {
			key = args_escape(key_string_lookup_key(bd->key));

			if (!repeat)
				r = "";
			else if (bd->flags & KEY_BINDING_REPEAT)
				r = "-r ";
			else
				r = "   ";
			tmpused = xsnprintf(tmp, tmpsize, "%s-T ", r);

			cp = utf8_padcstr(table->name, tablewidth);
			cplen = strlen(cp) + 1;
			while (tmpused + cplen + 1>= tmpsize) {
				tmpsize *= 2;
				tmp = xrealloc(tmp, tmpsize);
			}
			tmpused = strlcat(tmp, cp, tmpsize);
			tmpused = strlcat(tmp, " ", tmpsize);
			free(cp);

			cp = utf8_padcstr(key, keywidth);
			cplen = strlen(cp) + 1;
			while (tmpused + cplen + 1 >= tmpsize) {
				tmpsize *= 2;
				tmp = xrealloc(tmp, tmpsize);
			}
			tmpused = strlcat(tmp, cp, tmpsize);
			tmpused = strlcat(tmp, " ", tmpsize);
			free(cp);

			cp = cmd_list_print(bd->cmdlist, 1);
			cplen = strlen(cp);
			while (tmpused + cplen + 1 >= tmpsize) {
				tmpsize *= 2;
				tmp = xrealloc(tmp, tmpsize);
			}
			strlcat(tmp, cp, tmpsize);
			free(cp);

			cmdq_print(item, "bind-key %s", tmp);

			free(key);
			bd = key_bindings_next(table, bd);
		}
		table = key_bindings_next_table(table);
	}

	free(tmp);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_list_keys_commands(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	const struct cmd_entry	**entryp;
	const struct cmd_entry	 *entry;
	struct format_tree	 *ft;
	const char		 *template, *s;
	char			 *line;

	if ((template = args_get(args, 'F')) == NULL) {
		template = "#{command_list_name}"
		    "#{?command_list_alias, (#{command_list_alias}),} "
		    "#{command_list_usage}";
	}

	ft = format_create(item->client, item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, NULL, NULL, NULL);

	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		entry = *entryp;

		format_add(ft, "command_list_name", "%s", entry->name);
		if (entry->alias != NULL)
			s = entry->alias;
		else
			s = "";
		format_add(ft, "command_list_alias", "%s", s);
		if (entry->usage != NULL)
			s = entry->usage;
		else
			s = "";
		format_add(ft, "command_list_usage", "%s", s);

		line = format_expand(ft, template);
		if (*line != '\0')
			cmdq_print(item, "%s", line);
		free(line);
	}

	format_free(ft);
	return (CMD_RETURN_NORMAL);
}
