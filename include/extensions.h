/* ------------------------------------------------------------------------- *
 * Text Document Template Parser and Processor -- Extension Tag Implementation
 *
 * author:
 * Jamis Buck (jgb3@email.byu.edu)
 *
 * description:
 * This module implements "extension" tags -- tags that have been suggested
 * or implemented by others, but which would not be used often enough to
 * be included automatically in a new template manager.
 *
 * These tags are:
 *
 *   ESCAPE-JS -- treats all output from its embedded data as a javascript
 *     string, and escapes characters that need to be escaped (single and
 *     double quotes, most notably, but also backslash characters, newlines,
 *     carriage returns, and tabs).
 *
 *   ESCAPE-HTML -- treats all output from its embedded data as html
 *     and escapes characters that need to be escaped (less-than, greater-than,
 *     ampersand, quotes, etc.).
 *
 *   STRUCT -- this is like a repeat tag, but allows you to name the
 *     fields in each row and reference them by name (even multiple times, and in
 *     any order).  This also lets you use the fields in the conditional tags,
 *     and so forth.  Here's an example:
 *
 *       <table>
 *         <tr><th>Name</th><th>Email</th><th>Phone</th></tr>
 *         <!--%STRUCT=struct-fields=struct-data=##=
 *           <tr>
 *             <!--%IF=email=
 *               <td><a href="mailto:<!--%email%-->"><!--%name%--></a></td>
 *               <td><a href="mailto:<!--%email%-->"><!--%email%--></a></td>
 *             %-->
 *             <!--%IF_NOT=email=
 *               <td><!--%name%--></td>
 *               <td>&nbsp;</td>
 *             %-->
 *             <td><!--%phone%--></td>
 *           </tr>
 *         %-->
 *       </table>
 *
 *     The STRUCT tag takes 4 parameters: 'struct-fields', 'struct-data', the
 *     delimiter, and the output.
 *
 *     The 'struct-fields' is a delimited list of field names, one per
 *     column.  The fields names are delimited with the given delimiter.
 *     This may either be the list itself, or refer to a tag that contains
 *     the header data.
 *
 *     The 'struct-data' is a delimited list of data columns, with the columns
 *     given in the same order as the 'struct-fields' list.  This must refer
 *     to a tag that contains the header data.
 *
 *     The 'delimiter' is a string that is used to separate fields in the
 *     field name and data lists. This must be a literal string.
 *
 *     Using the above template, if 'struct-fields' is the following
 *     string:
 *     
 *       name##email##phone##
 *
 *     And if struct-data is the following string:
 *
 *       Jamis##jgb3#email.byu.edu##418-1470##Joe Student##js279@email.byu\
 *       .edu####Billy######
 *
 *     The output would be:
 *
 *       Name             Email                   Phone
 *       ----------------------------------------------
 *       <Jamis>          <jgb3@email.byu.edu>    418-1470
 *       <Joe Student>    <js279@email.byu.edu>
 *       Billy
 *
 * ------------------------------------------------------------------------- */

#ifndef __EXTENSIONS_H__
#define __EXTENSIONS_H__

#include "templates.h"

t_ae_tag ae_escape_js_tag( void );
t_ae_tag ae_escape_html_tag( void );
t_ae_tag ae_struct_tag( void );

#endif
