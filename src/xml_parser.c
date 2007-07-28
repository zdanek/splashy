/************************************************************************
 *                              xml_parser.c                            *
 *                                                                      *
 *  2005-04-15 18:58 EDT                                                *
 *  Copyright  2005  Luis Mondesi <lemsx1@gmail.com>                    *
 ************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 */

/*
 * A very basic parser based on glib's internal xml parser
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "common_macros.h"
#include "xml_parser.h"

static xml_parser_t *_xml_parser_db;    /* this holds all the tags parsed */

/*
 * private vars 
 */

static GQueue *_xml_parser_stack; /** keeps track of what tag was open last */
static GQueue *_xpath; /** fake xpath */

/*
 * helper functions 
 */
gboolean xml_parser_init (const gchar * _xml_file);

/*
 * function definitons 
 */

/**
 * @desc returns xpath like string from _xpath queue
 */
inline static void
xml_parser_append_xpath (gpointer data, gpointer user_data)
{
        const gchar *dir_sep = "/";     /* no need to ever change this. not a 
                                         * UNIX path */
        GString *_tmp_str = (GString *) user_data;
        _tmp_str = g_string_append (_tmp_str, dir_sep);
        _tmp_str =
                g_string_append (_tmp_str,
                                 g_strdup (((GString *) ((xml_tag_text_t *)
                                                         data)->tag)->str));
}

inline static const gchar *
xml_parser_get_xpath ()
{
        GString *_str = g_string_new ("");
        g_queue_foreach (_xpath, xml_parser_append_xpath, (gpointer) _str);
        const gchar *_ret_str = g_strdup (_str->str);
        g_string_free (_str, TRUE);
        return _ret_str;
}
inline static void
xml_parser_xpath_push (xml_tag_text_t * p)
{
        if (_xpath == NULL)
                _xpath = g_queue_new ();
        g_queue_push_tail (_xpath, (gpointer) p);
        DEBUG_PRINT ("XPATH: Pushing %p\n", p);
}
inline static void
xml_parser_xpath_pop ()
{
        xml_tag_text_t *_last = NULL;
        _last = g_queue_pop_tail (_xpath);
        DEBUG_PRINT ("XPATH: Poping %p\n", _last);
}
inline static void
xml_parser_stack_push (xml_node_t * p)
{
        if (_xml_parser_stack == NULL)
                _xml_parser_stack = g_queue_new ();
        g_queue_push_tail (_xml_parser_stack, (gpointer) p);
        DEBUG_PRINT ("STACK: Pushing %p\n", p);
}
inline static void
xml_parser_stack_pop ()
{
        xml_node_t *_last = NULL;
        _last = g_queue_pop_tail (_xml_parser_stack);
        DEBUG_PRINT ("STACK: Poping %p\n", _last);
}

static void
xml_parser_start_element_handler (GMarkupParseContext * context,
                                  const gchar * element_name,
                                  const gchar ** attribute_names,
                                  const gchar ** attribute_values,
                                  gpointer user_data, GError ** error)
{
        g_return_if_fail (element_name != NULL);

        DEBUG_PRINT ("ELEMENT: '%s'", element_name);

        gint i = 0;

        /*
         * use _list alias for gpointer user_data. less typing 
         */

        GList *_list = (GList *) user_data;

        /*
         * create bucket for this node 
         */
        xml_node_t *_node = g_new0 (xml_node_t, 1);
        /*
         * save name 
         */
        /*
         * save in xpath first 
         */
        xml_tag_text_t *_m_xpath = g_new0 (xml_tag_text_t, 1);
        _m_xpath->tag = g_string_new (element_name);
        xml_parser_xpath_push (_m_xpath);

        const gchar *_xpath_name = xml_parser_get_xpath ();

        _node->name = g_string_new (_xpath_name);

        /*
         * g_print("->name '%s' POINTER '%p'\n", _node->name->str,
         * _node->name); 
         */

        /*
         * init text to nothing. we will get this later 
         */
        _node->text = g_string_new ("");        /* blank string */
        /*
         * create bucket for attributes 
         */
        _node->attrs = g_new0 (GList, 1);
        /*
         * make some assumptions 
         */
        _node->got_attr = FALSE;
        _node->got_text = FALSE;
        /*
         * _node->tag_opened=TRUE; 
         */
        xml_parser_stack_push (_node);

        /*
         * if attributes are found, save for future reference 
         */
        if (attribute_names[0] != NULL)
        {
                _node->got_attr = TRUE;
        }

        while (_node->got_attr && attribute_names[i] != NULL)
        {
                DEBUG_PRINT ("NAME: %s", attribute_names[i]);
                DEBUG_PRINT ("VALUE: %s", attribute_values[i]);
                xml_attr_t *_attr = g_new0 (xml_attr_t, 1);
                _attr->name = g_string_new (attribute_names[i]);
                _attr->value = g_string_new (attribute_values[i]);
                _node->attrs = g_list_append (_node->attrs, (gpointer) _attr);
                ++i;
        }
        _xml_parser_db->priv = g_list_append (_list, (gpointer) _node);

}                               /* end xml_parser_start_element_handler */

static void
xml_parser_end_element_handler (GMarkupParseContext * context,
                                const gchar * element_name,
                                gpointer user_data, GError ** error)
{
        DEBUG_PRINT ("END: '%s'", element_name);
        GList *_list = g_list_last ((GList *) user_data);
        ((xml_node_t *) _list->data)->tag_opened = FALSE;
        xml_parser_stack_pop ();
        xml_parser_xpath_pop ();
}

static void
xml_parser_text_handler (GMarkupParseContext * context,
                         const gchar * text,
                         gsize text_len, gpointer user_data, GError ** error)
{
        g_return_if_fail (text_len > 0);
        /*
         * NOTE
         * Strings parse by gmarkup parser are not nul-terminated!
         */

        DEBUG_PRINT ("TEXT: '%s'", text);

        /*
         * assume that only one thread of this program is running, then the
         * last node appended to the list _list is the tag for which we will
         * be saving our text TODO find a better way to do this (if any)
         * remember to fix all other handlers 
         */
        xml_node_t *_last_node = NULL;
        _last_node = g_queue_peek_tail (_xml_parser_stack);
        if (_last_node != NULL)
        {
                _last_node->text = g_string_append (_last_node->text, text);
                /*
                 * g_print("->text '%s' POINTER '%p'\n",
                 * _last_node->text->str, _last_node->text); 
                 */
                _last_node->got_text = TRUE;
                DEBUG_PRINT ("text_handler() _last_node Tag '%s'",
                             _last_node->name->str);
                DEBUG_PRINT ("text_handler() _last_node Text '%s'",
                             _last_node->text->str);
        }
}

static void
xml_parser_passthrough_handler (GMarkupParseContext * context,
                                const gchar * passthrough_text,
                                gsize text_len,
                                gpointer user_data, GError ** error)
{
        DEBUG_PRINT ("PASS: '%s'", passthrough_text);
}

static void
xml_parser_error_handler (GMarkupParseContext * context,
                          GError * error, gpointer user_data)
{
        ERROR_PRINT ("%s", error->message);
}

/**
 * @desc a convenient function to return our internal XML db
 */
xml_parser_t *
xml_parser_get_db ()
{
        return _xml_parser_db;
}

/**
 * @desc a convenient function to set the XML file of our internal XML db
 *       to a new filename
 */
gboolean
xml_parser_set_xml_file (const gchar * xml_file)
{
        if (!g_file_test (xml_file, G_FILE_TEST_IS_REGULAR))
        {
                ERROR_PRINT
                        ("Setting XML file to <<%s>> failed. XML File not found",
                         xml_file);
                return FALSE;
        }
        if (_xml_parser_db != NULL)
        {
                if (_xml_parser_db->xml_file_path != NULL)
                {
                        g_string_free (_xml_parser_db->xml_file_path, TRUE);
                }
                _xml_parser_db->xml_file_path = g_string_new (xml_file);
        }
        return FALSE;
}

/*
 * our parser 
 */
static GMarkupParser xml_parser = {
        xml_parser_start_element_handler,
        xml_parser_end_element_handler,
        xml_parser_text_handler,
        xml_parser_passthrough_handler,
        xml_parser_error_handler
};

/**
 * @desc file parser routine 
 * Slurps all tags from a given filename and appends them to a XML db
 * @ingroup core
 * @param _filename filename path to read
 * @return true on success
 */
gboolean
xml_parser_read (xml_parser_t * xml_db)
{
        if (!g_file_test (xml_db->xml_file_path->str, G_FILE_TEST_IS_REGULAR))
        {
                ERROR_PRINT ("XML file not found <<%s>>",
                             xml_db->xml_file_path->str);
                return FALSE;
        }
        DEBUG_PRINT ("ENTERING xml_parser_read(%s)",
                     xml_db->xml_file_path->str);
        gchar *contents;
        gsize length;
        GError *error;
        GMarkupParseContext *context;

        error = NULL;

        if (!g_file_get_contents (xml_db->xml_file_path->str,
                                  &contents, &length, &error))
        {
                ERROR_PRINT ("%s", error->message);
                g_error_free (error);
                return FALSE;
        }

        context = g_markup_parse_context_new (&xml_parser, 0,
                                              xml_db->priv, NULL);
        if (!g_markup_parse_context_parse (context, contents, length, NULL))
        {
                g_markup_parse_context_free (context);
                return FALSE;
        }

        if (!g_markup_parse_context_end_parse (context, NULL))
        {
                g_markup_parse_context_free (context);
                return FALSE;
        }

        /*
         * free up memory 
         */
        g_markup_parse_context_free (context);
        g_free (contents);

        /*
         * now we have all the 'contents' we wanted 
         */
        DEBUG_PRINT ("EXITING xml_parser_read(%s)", "");
        return TRUE;            /* assuming that if we reach here, no error
                                 * happend */
}

/**
 * @desc search for tag in node list and sets the text buffer accordingly.
 */

void
_xml_parser_compare_element (gpointer data, gpointer user_data)
{
        /*
         * g_return_if_fail ( (GList*)data != NULL ); 
         */
        if ((GList *) data == NULL)
                return;

        xml_node_t *_node = (xml_node_t *) data;

        g_return_if_fail (_node != NULL);
        g_return_if_fail (_node->name != NULL);
        g_return_if_fail (_node->text != NULL);
        g_return_if_fail (_node->got_text == TRUE);

        GString *_tag = ((xml_tag_text_t *) user_data)->tag;
        GString *_name_tag = _node->name;

        if (g_string_equal (_tag, _name_tag))
        {
                ((xml_tag_text_t *) user_data)->text = _node->text->str;
        }
}                               /* end xml_parser_find_element */

/*
 * @desc Finds a given XML tag text and returns it as text.
 *       It assumes that there is only one xpath per string (unique keys); so,
 *       it will return the first one it finds.
 *       It also assumes that the XML file has been previously parsed using
 *       xml_parser_read(). @see xml_parser_read()
 * @param ctag XML tag to retrieve
 * @return XML tag's content as text
 */
const gchar *
xml_parser_get_text (const gchar * ctag)
{
        if (_xml_parser_db == NULL)
        {
                /*
                 * ERROR_PRINT ( "Failed to get text for xpath <<%s>>. XML db 
                 * has not been initialized",ctag ); return NULL;
                 */
                DEBUG_PRINT ("xml_parser_get_text(): initializing XML db %s",
                             "");
                xml_parser_read (_xml_parser_db);
        }

        GString *tag = g_string_new (ctag);

        xml_tag_text_t *_tag = g_new0 (xml_tag_text_t, 1);
        _tag->tag = tag;
        _tag->text = NULL;

        g_list_foreach (_xml_parser_db->priv,
                        _xml_parser_compare_element, (gpointer) _tag);

        return _tag->text;
}

/*
 * @desc Finds a given XML tag text and returns it as int.
 *       It assumes that there is only one xpath per string (unique keys); so,
 *       it will return the first one it finds.
 *       It also assumes that the XML file has been previously parsed using
 *       xml_parser_read(). @see xml_parser_read()
 * @param ctag XML tag to retrieve
 * @return XML tag's content as integer
 */
gint
xml_parser_get_int (const gchar * ctag, gint base)
{
        long ret = 0.0;
        gint _base = 0;         /* special base. strtol will do the right
                                 * thing */
        if (base >= 0 || base < 36)
        {
                _base = base;
        }
        const gchar *_text = xml_parser_get_text (ctag);
        errno = 0;              /* reset error before strtol() */
        if (_text != NULL)
                ret = strtol (_text, NULL, _base);
        else
                return -1;      /* error */

        if (errno)
        {
                ERROR_PRINT
                        ("Overflow occured while converting tag '%s' to int",
                         _text);
                return -1;
        }

        return (gint) ret;
}

gboolean
xml_parser_init (const gchar * _xml_file)
{
        if (_xml_parser_db != NULL)
        {
                /*
                 * This function should be called only once 
                 */
                DEBUG_PRINT ("XML parser was already init()'ed. Reusing %s",
                             "");
                return FALSE;
        }
        if (!g_file_test (_xml_file, G_FILE_TEST_IS_REGULAR))
        {
                ERROR_PRINT ("Cannot read XML File <<%s>>. Exiting...",
                             _xml_file);
                return FALSE;
        }
        _xml_parser_db = g_new0 (xml_parser_t, 1);
        _xml_parser_db->xml_file_path = g_string_new (_xml_file);
        _xml_parser_db->priv = g_new0 (GList, 1);
        if (_xml_parser_db != NULL && _xml_parser_db->priv != NULL)
                return xml_parser_read (_xml_parser_db);
        return FALSE;
}

/*
 * EOF 
 */

/*
 * FOR DEBUGGING
 * For testing compile with:
 * gcc -g -Wall `pkg-config --libs --cflags glib-2.0` -o test xml_parser.c
 * And then run ./test /path/to/xml/file [/path/to/file/2]
 */
/*int
main (int argc, char **argv)
{

        const gchar *tag = "/splashy/progressbar/dimension/x";
        const gchar *tag2 = "/splashy/background/boot";
        const gchar *tag3 = "/splashy/background/errorimg";

        const gchar *sz_text = NULL;
        long n_text = 0.0;

        if (!g_file_test (argv[1], G_FILE_TEST_IS_REGULAR))
                exit (1);

        xml_parser_init (argv[1]);

        if (g_file_test (argv[2], G_FILE_TEST_IS_REGULAR))
        {
                xml_parser_set_xml_file (argv[2]);
        }

        sz_text = xml_parser_get_text (tag);
        n_text = xml_parser_get_int (tag, 10);

        g_print ("main() '%s' = '%s'\n", tag, sz_text);
        g_print ("main() tag AS INT '%s' = '%i'\n", tag, (gint) n_text);

        sz_text = NULL;         // leaks
        sz_text = xml_parser_get_text (tag2);
        g_print ("main() '%s' = '%s'\n", tag2, sz_text);

        sz_text = NULL;         // leaks 
        sz_text = xml_parser_get_text (tag3);
        g_print ("main() '%s' = '%s'\n", tag3, sz_text);

        return 0;
}*/
