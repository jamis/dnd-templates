#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "extensions.h"

static int static_ae_escape_js_tag_process( t_ae_tag tag,
                                            CONST char* text,
                                            t_ae_template_mgr mgr,
                                            FILE* output );

static int static_ae_escape_html_tag_process( t_ae_tag tag,
                                              CONST char* text,
                                              t_ae_template_mgr mgr,
                                              FILE* output );

static char* static_ae_process_embedded_data( t_ae_tag tag,
                                              CONST char* text,
                                              t_ae_template_mgr mgr );

int static_ae_struct_tag_process( t_ae_tag tag,
                                  CONST char* text,
                                  t_ae_template_mgr mgr,
                                  FILE* output );


t_ae_tag ae_escape_js_tag( void ) /*{{{*/
{
  return ae_typed_tag( "ESCAPE-JS",
                       static_ae_escape_js_tag_process,
                       sizeof( t_ae_generic_tag ) );
}
/*}}}*/

t_ae_tag ae_escape_html_tag( void )/*{{{*/
{
  return ae_typed_tag( "ESCAPE-HTML",
                       static_ae_escape_html_tag_process,
                       sizeof( t_ae_generic_tag ) );
}
/*}}}*/

t_ae_tag ae_struct_tag( void ) /* {{{ */
{
  return ae_typed_tag( "STRUCT",
                       static_ae_struct_tag_process,
                       sizeof( t_ae_generic_tag ) );
}
/* }}} */


static int static_ae_escape_js_tag_process( t_ae_tag tag, /*{{{*/
                                            CONST char* text,
                                            t_ae_template_mgr mgr,
                                            FILE* output )
{
  char* buffer;
  char* p;

  /* process the embedded data into a temporary buffer */
  p = buffer = static_ae_process_embedded_data( tag, text, mgr );

  /* write the buffer, escaping illegal characters */
  while( *p ) {
    switch( *p ) {
      case '\'':
      case '"':
      case '\\':
        fputc( '\\', output );
        fputc( *p, output );
        break;
      case '\n':
        fputs( "\\n", output );
        break;
      case '\r':
        fputs( "\\r", output );
        break;
      case '\t':
        fputs( "\\t", output );
        break;
      default:
        fputc( *p, output );
    }
    p++;
  }

  free( buffer );
  return 1;
}
/*}}}*/

static int static_ae_escape_html_tag_process( t_ae_tag tag, /*{{{*/
                                              CONST char* text,
                                              t_ae_template_mgr mgr,
                                              FILE* output )
{
  char* buffer;
  char* p;

  /* process the embedded data into a temporary buffer */
  p = buffer = static_ae_process_embedded_data( tag, text, mgr );

  /* write the buffer, escaping illegal characters */
  while( *p ) {
    switch( *p ) {
      case '<': fputs( "&lt;", output ); break;
      case '>': fputs( "&gt;", output ); break;
      case '&': fputs( "&amp;", output ); break;
      case '"': fputs( "&quot;", output ); break;
      case '\'': fputs( "&#39;", output ); break;
      default:
        fputc( *p, output );
    }
    p++;
  }

  free( buffer );
  return 1;
}
/*}}}*/

static char* static_ae_process_embedded_data( t_ae_tag tag, /*{{{*/
                                              CONST char* text,
                                              t_ae_template_mgr mgr )
{
  FILE* tmp_dest;
  int   length;
  char* buffer;

  /* create a temporary file and process the embedded data into it */

  tmp_dest = tmpfile();
  ae_process_buffer( mgr, ae_get_field( text, ae_get_tag_delim( tag ), 1 ), tmp_dest );
  fflush( tmp_dest );
  length = ftell( tmp_dest );
  rewind( tmp_dest );

  /* allocate a buffer big enough to hold the data, read the data into it, close the
   * temporary file, and return the buffer */

  buffer = (char*)malloc( length+1 );
  fread( buffer, 1, length, tmp_dest );
  buffer[length] = 0;
  fclose( tmp_dest );

  return buffer;
}
/*}}}*/

int static_ae_struct_tag_process( t_ae_tag tag, /* {{{ */
                                  CONST char* text,
                                  t_ae_template_mgr mgr,
                                  FILE* output )
{
  char* hdr;
  char* data_tok;
  char* delim;
  char* data;
  char* hdrP;
  char* value;
  char* hdr_item;
  char* data_item;
  char* hdr_value;
  char* data_value;
  char* p;
  char  row_num_tag[ 18 ];
  int   max_hdr_len;
  int   max_data_len;
  int   row;

  /* <!--%STRUCT=hdr-tag=data-tag=delim=data%-->
   * <!--%STRUCT=hdr-list=data-tag=delim=data%-->
   * */

  /* hdr is a 'delim' delimited list of header fields.  For each iteration of the
   * loop, we add values to the manager with these names. */

  hdr = ae_get_field_alloc( text, ae_get_tag_delim( tag ), 1 );
  value = ae_get_value( mgr, hdr );
  if( value ) {
    free( hdr );
    hdr = strdup( value );
  }

  /* the data-tok is the name of the token that has the data to query for this
   * tag. */

  data_tok = ae_get_field_alloc( text, ae_get_tag_delim( tag ), 2 );
  value = ae_get_value( mgr, data_tok );
  free( data_tok );
  if( value ) {
    delim = ae_get_field_alloc( text, ae_get_tag_delim( tag ), 3 );
    data = ae_get_field( text, ae_get_tag_delim( tag ), 4 );

    /* determine the name of the tag that will identify this row */
    row = 1;
    strcpy( row_num_tag, "ae_row_number" );
    while( ae_get_tag( mgr, row_num_tag ) != NULL ) {
      row++;
      sprintf( row_num_tag, "ae_row_number_%d", row );
    }
 
    /* allocate the buffers that we'll use to hold the data and header info */
    max_hdr_len = 32;
    max_data_len = 32;
    hdr_value = (char*)malloc( max_hdr_len );
    data_value = (char*)malloc( max_data_len );

    row = 1;
    while( *value ) {
      ae_add_tag_i( mgr, row_num_tag, row );

      /* assign the token values to the manager */
      hdrP = hdr;
      while( *hdrP ) {
        hdr_item = strstr( hdrP, delim );
        if( hdr_item == NULL ) {
          fprintf( output, "header list is missing ending delimiter" );
          break;
        }
        data_item = strstr( value, delim );
        if( data_item == NULL ) {
          fprintf( output, "data list is missing ending delimiter" );
          break;
        }

        if( hdr_item - hdrP + 1 > max_hdr_len ) {
          max_hdr_len = hdr_item - hdrP + 1;
          free( hdr_value );
          hdr_value = (char*)malloc( max_hdr_len );
        }
        p = hdr_value;
        while( hdrP < hdr_item ) {
          *p = *hdrP;
          p++;
          hdrP++;
        }
        *p = 0;
        hdrP += strlen( delim );

        if( data_item - value + 1 > max_data_len ) {
          max_data_len = data_item - value + 1;
          free( data_value );
          data_value = (char*)malloc( max_data_len );
        }
        p = data_value;
        while( value < data_item ) {
          *p = *value;
          p++;
          value++;
        }
        *p = 0;
        value += strlen( delim );

        ae_add_tag( mgr, hdr_value, data_value );
      }

      ae_process_buffer( mgr, data, output );
      row++;
    }

    free( hdr_value );
    free( data_value );
  }

  free( hdr );
  return 1;
}
/* }}} */

