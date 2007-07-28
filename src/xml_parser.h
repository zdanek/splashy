/************************************************************************
 *                              xml_parser.h                            *
 *                                                                      *
 *  2005-05-28 03:36 EDT                                                *
 *  Copyright  2005  Luis Mondesi <lemsx1@gmail.com>                    *
 ************************************************************************/
/*
 * This file is part of Splashy.
 * 
 * Splashy is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any
 * later version.
 * 
 * Splashy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with Splashy; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301 USA 
 */
#ifndef _XML_PARSER_H
#define _XML_PARSER_H
#include <glib.h>

/** data structures */

/**
 * @desc used by xml_find_node when finding tag names in nodes
 */
typedef struct _tag_text
{
        GString *tag;
        const gchar *text;
} xml_tag_text_t;

typedef struct _attr
{
        GString *name;    /** attribute name */
        GString *value;   /** attribute value */
} xml_attr_t;

typedef struct _node
{
        GString *text;          /** XML text value, if any */
        GString *name;          /** XML tag name */
        gboolean got_attr;      /** does this node have attributes ? */
        gboolean got_text;      /** does this node have any text? */
        gboolean tag_opened;    /** status whether tag is open or close */
        guint depth;            /** how deep into the XML file are we? */
        GList *attrs;           /** list of xml_attr_t for this tag */
} xml_node_t;

typedef struct _xml_config
{
        GString *xml_file_path;         /** full path to the configuration file */
        GList *priv;                    /** list of xml_node_t for this config file */
} xml_parser_t;

/*
 * public functions 
 */
/*
 * changes the internal xml parser file name 
 */
gboolean xml_parser_set_xml_file (const gchar * xml_file);
/*
 * xml_parser_init must be call when initializing the parser. 
 */
gboolean xml_parser_init (const gchar * xml_file);
/*
 * returns our internal XML db 
 */
xml_parser_t *xml_parser_get_db ();
/*
 * appends tags to a XML db 
 */
gboolean xml_parser_read (xml_parser_t * xml_db);

/*
 * FIXME const gchar* xml_parser_find_element(const gchar* xpath); 
 */
const gchar *xml_parser_get_text (const gchar * ctag);
gint xml_parser_get_int (const gchar * ctag, gint base);

#endif /* _XML_PARSER_H */
