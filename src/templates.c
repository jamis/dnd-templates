#if defined( linux )
# define DLOPEN_TYPE
# include <dlfcn.h>
#elif defined( _HPUX_SOURCE )
# include <dl.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "templates.h"

/* ------------------------------------------------------------------------- */
/* macros and constants                                                      */
/* ------------------------------------------------------------------------- */

  /* we do this as a define since the replace tag and cyclical replace tag
   * use the same header... in other words, the replace tag inherits from
   * the standard tag, and the cyclical replace tag inherits from the replace
   * tag */

#define STANDARD_REPLACE_TAG_HDR \
  STANDARD_TAG_HDR; \
  char* m_data

#define DEFAULT_TAG_START "<!--%"
#define DEFAULT_TAG_END   "%-->"
#define DEFAULT_DELIMITER "="

#define NEW( item_name )     (item_name*)malloc( sizeof( item_name ) )
#define NEWTAG( name, type ) (type*)ae_tag_new( name, sizeof( type ) )
#define DELETE( x )          free( x )

#define DECL_CAST( var, parm, type )  type* var = (type*)parm
#define MGR_CAST( var, parm )         DECL_CAST( var, parm, t_ae_mgr )
#define GENERIC_TAG( var, parm )      DECL_CAST( var, parm, t_ae_generic_tag )

#define COMP_TYPE_EQ      ( 0 )
#define COMP_TYPE_NE      ( 1 )
#define COMP_TYPE_LT      ( 2 )
#define COMP_TYPE_LE      ( 3 )
#define COMP_TYPE_GT      ( 4 )
#define COMP_TYPE_GE      ( 5 )

/* ------------------------------------------------------------------------- */
/* type implementations                                                      */
/* ------------------------------------------------------------------------- */

typedef t_ae_tag (*t_standard_tag_def)();

typedef struct {
  STANDARD_STREAM_HDR;
} t_ae_generic_stream;

typedef struct {
  STANDARD_STREAM_HDR;
  FILE* fptr;
  int   wrapped;
} t_ae_file_stream;

typedef struct {
  STANDARD_STREAM_HDR;
  char* buffer;
  int   pos;
} t_ae_buffer_stream;

typedef struct {
  STANDARD_REPLACE_TAG_HDR;
} t_ae_replace_tag;

typedef struct {
  STANDARD_REPLACE_TAG_HDR;
  char* m_rpt_delim;
  char* m_next;
} t_ae_cyclical_replace_tag;

typedef struct {
  STANDARD_TAG_HDR;
  char* m_lib;
  char* m_func;
  void* m_cookie;
} t_ae_shared_fn_tag;

typedef struct {
  STANDARD_TAG_HDR;
  int comp_type;
} t_ae_comparison_tag;

typedef struct __ae_tag_list t_ae_tag_list;
struct __ae_tag_list {
  t_ae_tag_list*    next;
  t_ae_tag_list*    prev;
  t_ae_generic_tag* tag;
};

typedef struct {
  t_ae_tag_list* m_taglist_head;
  t_ae_tag_list* m_taglist_tail;
  char* m_tag_start;
  char* m_tag_end;
  char* m_tag_delimiter;
  t_ae_preproc_fn preproc;
  void* cookie;
  int recursive_depth;
} t_ae_mgr;

typedef struct __ae_cookie t_ae_cookie;
struct __ae_cookie {
  t_ae_cookie* next;
  char* name;
  char* value;
  int ttl;
};

typedef struct {
  int headers;
  int no_cache;
  t_ae_cookie* cookies;
  FILE* output;
} t_ae_html_proc_data;

/* ------------------------------------------------------------------------- */
/* static function definitions                                               */
/* ------------------------------------------------------------------------- */

static t_ae_file_stream*   static_ae_file_stream_new( void );
static t_ae_buffer_stream* static_ae_buffer_stream_new( void );

static int static_ae_file_stream_get_length( t_ae_stream stream );
static int static_ae_file_stream_read( t_ae_stream stream, char* buffer, int length );
static int static_ae_file_stream_close( t_ae_stream stream );

static int static_ae_buffer_stream_get_length( t_ae_stream stream );
static int static_ae_buffer_stream_read( t_ae_stream stream, char* buffer, int length );
static int static_ae_buffer_stream_close( t_ae_stream stream );

static t_ae_tag static_ae_comparison_tag( CONST char* name,
                                          int comparison );

static int static_ae_replace_tag_apply( t_ae_tag tag,
                                        CONST char* text,
                                        t_ae_template_mgr mgr,
                                        FILE* output );
static int static_ae_replace_tag_process( t_ae_tag tag,
                                          CONST char* text,
                                          t_ae_template_mgr mgr,
                                          FILE* output );
static int static_ae_replace_tag_cleanup( t_ae_tag tag );

static int static_ae_cyclical_replace_tag_process( t_ae_tag tag,
                                                   CONST char* text,
                                                   t_ae_template_mgr mgr,
                                                   FILE* output );
static int static_ae_cyclical_replace_tag_cleanup( t_ae_tag tag );
static int static_ae_shared_fn_tag_cleanup( t_ae_tag tag );

static int static_ae_typed_tag_apply( t_ae_tag tag,
                                      CONST char* text,
                                      t_ae_template_mgr mgr,
                                      FILE* output );

static int static_ae_if_tag_process( t_ae_tag tag,
                                     CONST char* text,
                                     t_ae_template_mgr mgr,
                                     FILE* output );
static int static_ae_if_not_tag_process( t_ae_tag tag,
                                         CONST char* text,
                                         t_ae_template_mgr mgr,
                                         FILE* output );
static int static_ae_include_tag_process( t_ae_tag tag,
                                          CONST char* text,
                                          t_ae_template_mgr mgr,
                                          FILE* output );
static int static_ae_repeat_tag_process( t_ae_tag tag,
                                         CONST char* text,
                                         t_ae_template_mgr mgr,
                                         FILE* output );
static int static_ae_env_tag_process( t_ae_tag tag,
                                      CONST char* text,
                                      t_ae_template_mgr mgr,
                                      FILE* output );
static int static_ae_exec_tag_process( t_ae_tag tag,
                                       CONST char* text,
                                       t_ae_template_mgr mgr,
                                       FILE* output );
static int static_ae_comparison_tag_process( t_ae_tag tag,
                                             CONST char* text,
                                             t_ae_template_mgr mgr,
                                             FILE* output );
static int static_ae_include_tag_named_process( t_ae_tag tag,
                                                CONST char* text,
                                                t_ae_template_mgr mgr,
                                                FILE* output );
           
static int static_ae_shared_fn_apply( t_ae_tag tag,
                                      CONST char* text,
                                      t_ae_template_mgr mgr,
                                      FILE* output );
static int static_ae_shared_fn_process( t_ae_tag tag,
                                        CONST char* text,
                                        t_ae_template_mgr mgr,
                                        FILE* output );

static int   static_html_preproc_fn( t_ae_template_mgr mgr, FILE* output );

static char* static_get_non_value( t_ae_tag tag );
static char* static_get_replace_tag_value( t_ae_tag tag );

/* ------------------------------------------------------------------------- */
/* static data                                                               */
/* ------------------------------------------------------------------------- */

static t_standard_tag_def static_standard_tags[] = {
  ae_if_tag,
  ae_if_not_tag,
  ae_if_eq_tag,
  ae_if_not_eq_tag,
  ae_if_lt_tag,
  ae_if_le_tag,
  ae_if_gt_tag,
  ae_if_ge_tag,
  ae_include_tag,
  ae_repeat_tag,
  ae_env_tag,
  ae_exec_tag,
  0
};

/* ------------------------------------------------------------------------- */
/* stream function implementations                                           */
/* ------------------------------------------------------------------------- */

t_ae_stream ae_stream_open_file( CONST char* file_name ) {
  t_ae_file_stream* stream;

  /* create a new file stream object and open the requested file */
  stream = static_ae_file_stream_new();
  stream->fptr = fopen( file_name, "r" );
  if( stream->fptr == NULL ) {
    stream->close( stream );
    return NULL;
  }

  return (t_ae_stream)stream;
}

t_ae_stream ae_stream_wrap_file( FILE* fptr ) {
  t_ae_file_stream* stream;

  /* create a new file stream object and set the file pointer to be the one
   * indicated */

  stream = static_ae_file_stream_new();
  stream->fptr = fptr;
  stream->wrapped = 1;

  return (t_ae_stream)stream;
}

t_ae_stream ae_stream_open_buffer( CONST char* buffer ) {
  t_ae_buffer_stream* stream;

  stream = static_ae_buffer_stream_new();
  stream->buffer = strdup( buffer );
  stream->pos = 0;

  return (t_ae_stream)stream;
}

/* the following functions use the function pointers in the stream
 * structure to implement "virtual" methods of the stream object. */

int ae_stream_get_length( t_ae_stream stream ) {
  DECL_CAST( str, stream, t_ae_generic_stream );
  return str->get_length( stream );
}

int ae_stream_read( t_ae_stream stream, char* buffer, int length ) {
  DECL_CAST( str, stream, t_ae_generic_stream );
  return str->read( stream, buffer, length );
}

int ae_stream_close( t_ae_stream stream ) {
  DECL_CAST( str, stream, t_ae_generic_stream );
  str->close( stream );
  free( str );
  return 0;
}

/* ------------------------------------------------------------------------- */
/* tag function implementations                                              */
/* ------------------------------------------------------------------------- */

t_ae_tag ae_tag_new( CONST char* name, int size ) {
  t_ae_generic_tag* tag;

  /* create a new tag by allocating space for it, setting it's name and
   * delimiter, and setting default values for it's methods. */

  tag = (t_ae_generic_tag*)malloc( size );
  tag->m_tag = strdup( name );
  tag->m_delim = strdup( DEFAULT_DELIMITER );
  tag->apply = NULL;
  tag->process = NULL;
  tag->cleanup = NULL;
  tag->get_value = static_get_non_value;

  /* setting the type to TAG_TYPE_NO_VALUE means that the tag is untyped --
   * if you call it's get-value function, you will always get NULL back. */

  tag->type = TAG_TYPE_NO_VALUE;

  return (t_ae_tag)tag;
}

void ae_tag_destroy( t_ae_tag tag ) {
  GENERIC_TAG( tag_data, tag );

  /* if the tag has a cleanup function defined, call it */

  if( tag_data->cleanup ) {
    tag_data->cleanup( tag );
  }

  /* free the memory for the tag */

  free( tag_data->m_tag );
  free( tag_data->m_delim );
  free( tag_data );
}

char* ae_get_tag_name( t_ae_tag tag ) {
  GENERIC_TAG( tag_data, tag );
  return tag_data->m_tag;
}

char* ae_get_tag_delim( t_ae_tag tag ) {
  GENERIC_TAG( tag_data, tag );
  return tag_data->m_delim;
}

char* ae_get_tag_value( t_ae_tag tag ) {
  GENERIC_TAG( tag_data, tag );
  return tag_data->get_value( tag );
}


t_ae_tag ae_replace_tag( CONST char* name, CONST char* data ) {
  t_ae_replace_tag* tag;

  /* a replace tag simply replaces itself with the specified data.
   * If the data is NULL, the tag is replaced with the empty string. */

  tag = NEWTAG( name, t_ae_replace_tag );
  if( data != NULL ) {
/*    tag->m_data = strdup( data );*/
    tag->m_data = (char*)malloc( strlen( data ) + 1 );
    strcpy( tag->m_data, data );
  } else {
    tag->m_data = NULL;
  }
  tag->apply = static_ae_replace_tag_apply;
  tag->process = static_ae_replace_tag_process;
  tag->cleanup = static_ae_replace_tag_cleanup;
  tag->get_value = static_get_replace_tag_value;
  tag->type = TAG_TYPE_VALUE;

  return (t_ae_tag)tag;
}

t_ae_tag ae_cyclical_replace_tag( CONST char* name, CONST char* data, CONST char* delim ) {
  t_ae_cyclical_replace_tag* tag;

  /* a cyclical replace tag replaces itself with the next value in the associated delimited
   * list of values.  Each time the cyclical tag's "process" function is called, it's
   * m_next pointer is incremented so that on each subsequent call, the next value in the
   * list is obtained. */

  tag = NEWTAG( name, t_ae_cyclical_replace_tag );
  tag->m_data = strdup( data );
  tag->m_rpt_delim = strdup( delim );
  tag->m_next = tag->m_data;
  tag->apply = static_ae_replace_tag_apply;
  tag->process = static_ae_cyclical_replace_tag_process;
  tag->cleanup = static_ae_cyclical_replace_tag_cleanup;
  tag->get_value = static_get_replace_tag_value;
  tag->type = TAG_TYPE_VALUE;

  return (t_ae_tag)tag;
}

t_ae_tag ae_typed_tag( CONST char* type, t_ae_tag_fn process, int size ) {
  t_ae_generic_tag* tag;

  tag = (t_ae_generic_tag*)ae_tag_new( type, size );
  tag->apply = static_ae_typed_tag_apply;
  tag->process = process;

  return (t_ae_tag)tag;
}

t_ae_tag ae_if_tag( void ) {
  return ae_typed_tag( "IF", static_ae_if_tag_process, sizeof( t_ae_generic_tag ) );
}

t_ae_tag ae_if_not_tag( void ) {
  return ae_typed_tag( "IF_NOT", static_ae_if_not_tag_process, sizeof( t_ae_generic_tag ) );
}

t_ae_tag ae_if_eq_tag( void ) {
  return static_ae_comparison_tag( "IF_EQ", COMP_TYPE_EQ );
}

t_ae_tag ae_if_not_eq_tag( void ) {
  return static_ae_comparison_tag( "IF_NOT_EQ", COMP_TYPE_NE );
}

t_ae_tag ae_if_lt_tag( void ) {
  return static_ae_comparison_tag( "IF_LT", COMP_TYPE_LT );
}

t_ae_tag ae_if_le_tag( void ) {
  return static_ae_comparison_tag( "IF_LE", COMP_TYPE_LE );
}

t_ae_tag ae_if_gt_tag( void ) {
  return static_ae_comparison_tag( "IF_GT", COMP_TYPE_GT );
}

t_ae_tag ae_if_ge_tag( void ) {
  return static_ae_comparison_tag( "IF_GE", COMP_TYPE_GE );
}

t_ae_tag ae_include_tag( void ) {
  return ae_typed_tag( "INCLUDE", static_ae_include_tag_process, sizeof( t_ae_generic_tag ) );
}

t_ae_tag ae_repeat_tag( void ) {
  return ae_typed_tag( "REPEAT2", static_ae_repeat_tag_process, sizeof( t_ae_generic_tag ) );
}

t_ae_tag ae_env_tag( void ) {
  return ae_typed_tag( "ENV", static_ae_env_tag_process, sizeof( t_ae_generic_tag ) );
}

t_ae_tag ae_exec_tag( void ) {
  return ae_typed_tag( "EXEC", static_ae_exec_tag_process, sizeof( t_ae_generic_tag ) );
}

t_ae_tag ae_shared_fn_tag( CONST char* name, CONST char* lib, CONST char* func, void* cookie ) {
  t_ae_shared_fn_tag* tag;

  tag = NEWTAG( name, t_ae_shared_fn_tag );
  tag->apply = static_ae_shared_fn_apply;
  tag->process = static_ae_shared_fn_process;
  tag->cleanup = static_ae_shared_fn_tag_cleanup;
  tag->m_lib = strdup( lib );
  tag->m_func = strdup( func );
  tag->m_cookie = cookie;

  return (t_ae_tag)tag;
}

t_ae_tag ae_shared_fn_tag_named( CONST char* name, CONST char* lib,
                                 CONST char* func, void* cookie )
{
  t_ae_shared_fn_tag* tag;

  tag = (t_ae_shared_fn_tag*)ae_shared_fn_tag( name, lib, func, cookie );
  tag->apply = static_ae_replace_tag_apply;

  return (t_ae_tag)tag;
}

t_ae_tag ae_include_tag_named( CONST char* name, CONST char* file ) {
  t_ae_replace_tag* tag;

  tag = (t_ae_replace_tag*)ae_replace_tag( name, file );
  tag->process = static_ae_include_tag_named_process;

  return (t_ae_tag)tag;
}

/* ------------------------------------------------------------------------- */
/* template function implementations                                         */
/* ------------------------------------------------------------------------- */

t_ae_template_mgr ae_template_mgr_new( void ) {
  t_ae_mgr* mgr_data;
  int i;

  mgr_data = NEW( t_ae_mgr );
  mgr_data->m_taglist_head = NULL;
  mgr_data->m_taglist_tail = NULL;
  mgr_data->m_tag_start = strdup( DEFAULT_TAG_START );
  mgr_data->m_tag_end = strdup( DEFAULT_TAG_END );
  mgr_data->m_tag_delimiter = strdup( DEFAULT_DELIMITER );
  mgr_data->preproc = NULL;
  mgr_data->cookie = NULL;
  mgr_data->recursive_depth = 0;

  /* add the standard tag types, defined in the static_standard_tags array */
  for( i = 0; static_standard_tags[i] != NULL; i++ ) {
    ae_add_tag_ex( (t_ae_template_mgr)mgr_data, static_standard_tags[i]() );
  }

  return (t_ae_template_mgr)mgr_data;
}

void ae_template_mgr_done( t_ae_template_mgr mgr ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* curr;
  t_ae_tag_list* next;

  free( mgr_data->m_tag_start );
  free( mgr_data->m_tag_end );
  free( mgr_data->m_tag_delimiter );

  /* destroy the tags associated with this manager */
  curr = mgr_data->m_taglist_head;
  while( curr != NULL ) {
    ae_tag_destroy( (t_ae_tag)curr->tag );
    next = curr->next;
    free( curr );
    curr = next;
  }

  mgr_data->m_taglist_head = NULL;
  mgr_data->m_taglist_tail = NULL;

  free( mgr_data );
}

void ae_add_tag( t_ae_template_mgr mgr, CONST char* name, CONST char* value ) {
  /* add the name/value pair as a replace tag */
  ae_add_tag_ex( mgr, ae_replace_tag( name, value ) );
}

void ae_add_tag_i( t_ae_template_mgr mgr, CONST char* name, int value ) {
  char buffer[ 12 ];

  /* add the name/value pair as a replace tag */
  snprintf( buffer, sizeof( buffer ), "%d", value );
  ae_add_tag_ex( mgr, ae_replace_tag( name, buffer ) );
}

void ae_add_tags( t_ae_template_mgr mgr, char** names, char** values ) {
  int i;
  
  /* for each value in the given names/values arrays, add a tag.  If the
   * tag starts with a '-', then add the value as if it were a t_ae_tag
   * pointer, otherwise add it as a generic replace tag */

  if( names == 0 ) return;
  for( i = 0; names[i] && *names[i]; i++ ) {
    if( *names[i] == '-' ) {
      ae_add_tag_ex( mgr, (t_ae_tag)values[i] );
    } else {
      ae_add_tag( mgr, names[ i ], values[ i ] );
    }
  }
}

void ae_add_tag_ex( t_ae_template_mgr mgr, t_ae_tag tag ) {
  MGR_CAST( mgr_data, mgr );
  GENERIC_TAG( tag_data, tag );
  t_ae_tag_list* item;
  t_ae_tag_list* c;

  /* add the given tag to the manager's linked list of tags.  The
   * most recently added tag is added at the end of the list, and
   * the m_taglist_head and m_taglist_tail variables keep track of
   * (respectively) the head and tail of the list */

  if( tag == NULL ) return;
  ae_remove_tag( mgr, ae_get_tag_name( tag ) );

  free( tag_data->m_delim );
  tag_data->m_delim = strdup( mgr_data->m_tag_delimiter );

  c = mgr_data->m_taglist_tail;
  if( c == NULL ) {
    item = NEW( t_ae_tag_list );
    item->next = item->prev = NULL;
    item->tag = tag_data;
    mgr_data->m_taglist_tail = mgr_data->m_taglist_head = item;
  } else {
    while( c != NULL ) {
      if( strcmp( c->tag->m_tag, tag_data->m_tag ) == 0 ) {
        ae_tag_destroy( c->tag );
        c->tag = tag_data;
        break;
      }
      c = c->next;
    }
    if( c == NULL ) {
      item = NEW( t_ae_tag_list );
      item->next = NULL;
      item->prev = mgr_data->m_taglist_tail;
      item->prev->next = item;
      item->tag = tag_data;
      mgr_data->m_taglist_tail = item;
    }
  }
}

void ae_remove_tag( t_ae_template_mgr mgr, CONST char* name ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* item;

  /* remove the first tag found with the given name.  The tag
   * will be destroyed. */

  item = mgr_data->m_taglist_head;
  while( item != NULL ) {
    if( strcmp( item->tag->m_tag, name ) == 0 ) {
      if( item->prev != NULL ) {
        item->prev->next = item->next;
      }
      if( item->next != NULL ) {
        item->next->prev = item->prev;
      }
      if( item == mgr_data->m_taglist_tail ) {
        mgr_data->m_taglist_tail = item->prev;
      }
      if( item == mgr_data->m_taglist_head ) {
        mgr_data->m_taglist_head = item->next;
      }
      ae_tag_destroy( item->tag );
      free( item );
      break;
    }
    item = item->next;
  }
}

void ae_remove_tag_ex( t_ae_template_mgr mgr, t_ae_tag tag ) {
  /* remove the first tag answering to the same name as the given tag */
  ae_remove_tag( mgr, ((t_ae_generic_tag*)tag)->m_tag );
}

int ae_tag_count( t_ae_template_mgr mgr ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* item;
  int count = 0;

  for( item = mgr_data->m_taglist_head; item != NULL; item = item->next ) {
    count++;
  }

  return count;
}

void ae_set_start_end_delim( t_ae_template_mgr mgr,
                             CONST char* start,
                             CONST char* end )
{
  MGR_CAST( mgr_data, mgr );

  free( mgr_data->m_tag_start );
  free( mgr_data->m_tag_end );

  mgr_data->m_tag_start = strdup( start );
  mgr_data->m_tag_end   = strdup( end );
}

t_ae_tag ae_get_tag( t_ae_template_mgr mgr, CONST char* name ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* item;

  /* return the first tag answering to the given name */
  item = mgr_data->m_taglist_head;
  while( item != NULL ) {
    if( strcmp( item->tag->m_tag, name ) == 0 ) {
      return (t_ae_tag)item->tag;
    }
    item = item->next;
  }

  return NULL;
}

char* ae_get_value( t_ae_template_mgr mgr, CONST char* name ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* item;

  /* return the value of the first tag answering to the given name */
  item = mgr_data->m_taglist_head;
  while( item != NULL ) {
    if( strcmp( item->tag->m_tag, name ) == 0 ) {
      if( item->tag->type != TAG_TYPE_VALUE ) return NULL;
      return item->tag->get_value( (t_ae_tag)item->tag );
    }
    item = item->next;
  }

  return NULL;
}

t_ae_tag ae_get_tag_at( t_ae_template_mgr mgr, int index ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* item;

  item = mgr_data->m_taglist_head;
  while( index > 0 && item != NULL ) {
    item = item->next;
    index--;
  }

  if( item != NULL ) return item->tag;

  return item;
}

int ae_process_template( t_ae_template_mgr mgr, CONST char* file, FILE* output ) {
  t_ae_stream stream;
  int rc;

  /* open a file stream for the given file-name, and process the stream */

  stream = ae_stream_open_file( file );
  if( stream == NULL ) {
    return -1;
  }
  rc = ae_process_stream( mgr, stream, output );
  ae_stream_close( stream );

  return rc;
}

int ae_process_buffer( t_ae_template_mgr mgr, CONST char* buffer, FILE* output ) {
  t_ae_stream stream;
  int rc;

  /* open a buffer stream for the given buffer, and process the stream */

  stream = ae_stream_open_buffer( buffer );
  if( stream == NULL ) {
    return -1;
  }
  rc = ae_process_stream( mgr, stream, output );
  ae_stream_close( stream );

  return rc;
}

int ae_process_stream( t_ae_template_mgr mgr, t_ae_stream stream, FILE* output ) {
  MGR_CAST( mgr_data, mgr );
  t_ae_tag_list* item;
  char* text;
  char* data;
  char* start;
  char* last_start;
  char* end;
  int   size;
  int   start_delim_len;
  int   end_delim_len;
  int   rc = 0;
  int   original_fd = -1;

  /* if a preprocessing function has been specified, use it */
  if( mgr_data->recursive_depth < 1 && mgr_data->preproc != NULL ) {
    original_fd = ae_redirect_to( output, STDOUT_FILENO );
    mgr_data->preproc( mgr, output );
  }

  /* increment the recursive depth */
  mgr_data->recursive_depth++;

  /* precompute the length of the start and end delimiters */
  start_delim_len = strlen( mgr_data->m_tag_start );
  end_delim_len = strlen( mgr_data->m_tag_end );

  /* read the entire stream into a buffer */
  size = ae_stream_get_length( stream );
  data = (char*)malloc( size+1 );
  ae_stream_read( stream, data, size+1 );
  data[ size ] = 0;

  /* search through the text of the stream, replacing tags as they are encountered */
  text = data;
  start = strstr( text, mgr_data->m_tag_start );
  while( start != NULL ) {
    *start = 0;
    fputs( text, output );

    /* find the next end-token */
    end = strstr( start + start_delim_len, mgr_data->m_tag_end );
    if( end == NULL ) {
      fputs( "[unclosed tag]", output );
      rc = -1;
      break;
    }

    /* skip past nested tags, by looking for start-tags that begin after the current
     * start position, but before the next end-token. That is to say, if the tag delimiters
     * are <% and %>:
     *   <%   <%   <%  %>   %>       %>
     *   ^    ^        ^
     *   | last_start  |
     *   start         end
     * Here, start is the first start-token found, and end is the first end-token found.
     * Nested tags are detected because last_start exists between start and end. */

    last_start = strstr( start + start_delim_len, mgr_data->m_tag_start );
    while( last_start != NULL && last_start < end ) {
      /* We've found a nested token, so we skip it by looking for the next 'last_start' tag
       * AND the next 'end' tag, and we continue the loop if the last_start tag is before the
       * end tag.  In other words:
       *   End of Iteration #1  <%   <%   <%  %>   %>       %>
       *                        S         L        E
       *   End of Iteration #2  <%   <%   <%  %>   %>       %>
       *                        S                           E   ... L
       * Thus, by the end of iteration #2, last_start is either NULL or after E, which
       * means that the stretch of data from S to E completely contains all tags within
       * it, with no tags overlapping the ends of S-E. */

      end = strstr( end + end_delim_len, mgr_data->m_tag_end );
      if( end == NULL ) break;
      last_start = strstr( last_start + start_delim_len, mgr_data->m_tag_start );
    }

    /* if end is NULL, then the tag was not closed */
    if( end == NULL ) {
      fputs( "[unclosed tag]", output );
      rc = -1;
      break;
    }

    /* skip past the starting delimiter */
    start = start + start_delim_len;
    *end = 0;

    /* look for the first tag that can apply the given tag text.  Each tag contains
     * the logic it needs to recognize itself at the head of a chunk of text ('start'). */
    for( item = mgr_data->m_taglist_head; item != NULL; item = item->next ) {
      if( item->tag->apply( item->tag, start, mgr, output ) ) {
        break;
      }
    }

    /* start the next loop after the end of the ending delimiter */
    text = end + end_delim_len;

    /* look for the next starting delimiter */
    start = strstr( text, mgr_data->m_tag_start );
  }

  /* write the remaining data */
  fputs( text, output );
  free( data );

  /* decrement the recursive depth, as we are now leaving this function */
  mgr_data->recursive_depth--;
  if( mgr_data->recursive_depth < 1 ) {
    ae_restore_file( original_fd, stdout );
  }

  return rc;
}

void ae_set_preprocessor_func( t_ae_template_mgr mgr, t_ae_preproc_fn func ) {
  MGR_CAST( mgr_data, mgr );
  mgr_data->preproc = func;
}

void ae_set_mgr_cookie( t_ae_template_mgr mgr, void* cookie ) {
  MGR_CAST( mgr_data, mgr );
  mgr_data->cookie = cookie;
}

void* ae_get_mgr_cookie( t_ae_template_mgr mgr ) {
  MGR_CAST( mgr_data, mgr );
  return mgr_data->cookie;
}


/* ------------------------------------------------------------------------- */
/* ToHTML Replacement Functions                                              */
/* ------------------------------------------------------------------------- */

int ToHTML2( CONST char* tem_file, char** tokens, char** values, int mode ) {
  return ae_done_html( ae_init_html( tokens, values, mode ), tem_file );
}


t_ae_template_mgr ae_init_html( char** tokens, char** values, int mode ) {
  t_ae_template_mgr mgr;
  t_ae_html_proc_data* data;

  /* set up the HTML proc data structure, with fields' values depending on the value of mode */
  data = NEW( t_ae_html_proc_data );
  data->headers = ( mode & HTML_HEADER );
  data->no_cache = ( mode & HTML_NO_CACHE );
  data->cookies = NULL;

  /* default the output to stdout, but see ae_set_html_output */
  data->output = stdout;

  /* create the template manager */
  mgr = ae_template_mgr_new();

  /* set the HTML proc data structure in the template manager */
  ae_set_mgr_cookie( mgr, data );

  /* set the HTML preprocessor function (to handle writing of headers */
  ae_set_preprocessor_func( mgr, static_html_preproc_fn );

  /* add the token/value pairs */
  ae_add_tags( mgr, tokens, values );

  return mgr;
}


int ae_done_html( t_ae_template_mgr mgr, CONST char* tem_file ) {
  int rc = 0;
  t_ae_html_proc_data* data;
  t_ae_cookie* cookie;
  t_ae_cookie* next;

  data = (t_ae_html_proc_data*)ae_get_mgr_cookie( mgr );

  /* if a template file is specified, process it */
  if( tem_file != NULL ) {
    rc = ae_process_template( mgr, tem_file, data->output );
  }

  /* free the cookie list */
  cookie = data->cookies;
  while( cookie != NULL ) {
    free( cookie->name );
    free( cookie->value );
    next = cookie->next;
    free( cookie );
    cookie = next;
  }

  /* free the data */
  free( data );

  /* cleanup the manager */
  ae_template_mgr_done( mgr );

  /* convert return code into boolean -- '1' means success, '0' means failure.
   * This is left over from the original ToHTML implementation. */
  return ( rc == 0 ? 1 : 0 );
}


void ae_set_cookie( t_ae_template_mgr mgr,
                    CONST char* name,
                    CONST char* value,
                    int ttl )
{
  t_ae_html_proc_data* data;
  t_ae_cookie* cookie;

  data = (t_ae_html_proc_data*)ae_get_mgr_cookie( mgr );
  cookie = NEW( t_ae_cookie );

  cookie->name = strdup( name );
  cookie->value = strdup( value );
  cookie->ttl = ttl;
  cookie->next = data->cookies;

  data->cookies = cookie;
}


void ae_set_html_output( t_ae_template_mgr mgr, FILE* output ) {
  t_ae_html_proc_data* data;
  data = (t_ae_html_proc_data*)ae_get_mgr_cookie( mgr );
  data->output = output;
}

/* ------------------------------------------------------------------------- */
/* miscellaneous utility functions                                           */
/* ------------------------------------------------------------------------- */

char* ae_get_field( CONST char* text, CONST char* delim, int which ) {
  DECL_CAST( ptr, text, char );

  while( which > 0 ) {
    ptr = strstr( ptr, delim );
    if( ptr == NULL ) break;
    ptr += strlen( delim );
    which--;
  }

  return ptr;
}

char* ae_get_field_alloc( CONST char* text, CONST char* delim, int which ) {
  char* ptr;
  char* new_ptr;
  int   len;

  ptr = ae_get_field( text, delim, which );
  if( ptr == NULL ) return NULL;
  len = ae_field_len( ptr, delim );
  new_ptr = (char*)malloc( len+1 );
  ae_field_cpy( new_ptr, ptr, delim );

  return new_ptr;
}

int ae_field_cmp( CONST char* field, CONST char* text, CONST char* delim ) {
  char* end;
  char* fptr;
  char* tptr;

  end = strstr( field, delim );
  if( end == NULL ) end = (char*)field+strlen(field);

  /* look for the end of one of the strings, or the first point where they differ */
  for( fptr = (char*)field, tptr = (char*)text; *tptr && *fptr == *tptr && fptr < end; fptr++, tptr++ ) {
    /* do nothing */
  }

  if( fptr == end && *tptr == 0 ) return 0;
  if( *tptr == 0 ) return 1;
  return -1;
}

int ae_field_len( CONST char* field, CONST char* delim ) {
  char* end;

  end = strstr( field, delim );
  if( end == NULL ) end = (char*)field+strlen(field);

  return (int)( end - field );
}

char* ae_field_cpy( char* dest, CONST char* field, CONST char* delim ) {
  char* end;

  end = strstr( field, delim );
  if( end == NULL ) end = (char*)field+strlen(field);

  memcpy( dest, (char*)field, (int)(end-field) );
  dest[ (int)(end-field) ] = 0;

  return dest;
}

void* ae_load_dynamic_function( CONST char* lib, CONST char* func ) {
  void* func_ptr = NULL;
  int load_flags;

#if defined( DLOPEN_TYPE )
  void* lib_handle;

  load_flags = RTLD_NOW;
  if( !( lib_handle = dlopen( lib, load_flags ) ) ) {
    fprintf( stderr, "[could not load shared library '%s': %s]", lib, dlerror() );
    return NULL;
  }

  if( !( func_ptr = dlsym( lib_handle, func ) ) ) {
    fprintf( stderr, "[could not load entry point '%s:%s', %s]", lib, func, dlerror() );
    return NULL;
  }
#elif defined( _HPUX_SOURCE )
  shl_t lib_handle;

  load_flags = BIND_IMMEDIATE | BIND_VERBOSE;
  if( !(lib_handle = shl_load( lib, load_flags, 0 ) ) ) {
    fprintf( stderr, "[could not load shared library '%s']", lib );
    return NULL;
  }

  if( shl_findsym( &lib_handle, func, TYPE_PROCEDURE, (void*)&func_ptr ) ) {
    fprintf( stderr, "[could not load entry point '%s:%s']", lib, func );
    return NULL;
  }
#endif

  return func_ptr;
}

char* ae_build_library_name( char* dest, CONST char* root ) {
  char* buffer;
  char  cwd[ 256 ];
  char* sfx;
  char* webroot;

  buffer = dest;
  if( buffer == NULL ) {
    buffer = (char*)malloc( 256 );
  }

#if defined( _HPUX_SOURCE )
  /* with AE's framework, all libs are in /opt/<stage>/lib, where <stage> is
   * 'dev', 'tst', or 'prod'.  Depending on what the working directory is,
   * figure out which <stage> we are in and look for the library in that
   * library dir. */
  getcwd( cwd, sizeof( cwd ) );
  webroot = strstr(cwd, "web");
  if(webroot != NULL)
  {
    *webroot = '\0';
    strcpy( buffer, cwd );
  }
  else
  {
    strcpy( buffer, "/opt/" );
    if( strstr( cwd, "/prod/" ) ) {
      strcat( buffer, "prod/" );
    } else if( strstr( cwd, "/tst/" ) ) {
      strcat( buffer, "tst/" );
    } else {
      strcat( buffer, "dev/" );
    }
  }
  strcat( buffer, "lib/" );
  sfx = ".sl";
#else
  strcpy( buffer, "" );
  sfx = ".so";
#endif

  strcat( buffer, "lib" );
  strcat( buffer, root );
  strcat( buffer, sfx );

  return buffer;
}


int ae_redirect_to( FILE* output, int original_fd ) {
  int old_fd = -1;

  /* if 'output' is not already 'original_fd'... */
  if( fileno( output ) != original_fd ) {
    /* flush any cached data on output */
    fflush( output );
    /* copy original_fd */
    old_fd = dup( original_fd );
    /* set original_fd to be the same as 'output' */
    dup2( fileno( output ), original_fd );
  }
  return old_fd;
}


void ae_restore_file( int original_fd, FILE* file ) {
  /* if original_fd is valid... */
  if( original_fd >= 0 ) {
    /* flush any cached data on 'file' */
    fflush( file );
    /* set file to be a copy of 'original_fd' */
    dup2( original_fd, fileno( file ) );
    /* close 'original_fd' */
    close( original_fd );
  }
}

/* ------------------------------------------------------------------------- */
/* static function implementations                                           */
/* ------------------------------------------------------------------------- */

static t_ae_file_stream* static_ae_file_stream_new( void ) {
  t_ae_file_stream* ptr;
  ptr = (t_ae_file_stream*)malloc( sizeof( t_ae_file_stream ) );
  ptr->get_length = static_ae_file_stream_get_length;
  ptr->read = static_ae_file_stream_read;
  ptr->close = static_ae_file_stream_close;
  ptr->wrapped = 0;
  ptr->fptr = NULL;
  return ptr;
}

static t_ae_buffer_stream* static_ae_buffer_stream_new( void ) {
  t_ae_buffer_stream* ptr;
  ptr = (t_ae_buffer_stream*)malloc( sizeof( t_ae_buffer_stream ) );
  ptr->get_length = static_ae_buffer_stream_get_length;
  ptr->read = static_ae_buffer_stream_read;
  ptr->close = static_ae_buffer_stream_close;
  ptr->buffer = NULL;
  ptr->pos = 0;
  return ptr;
}

static int static_ae_file_stream_get_length( t_ae_stream stream ) {
  DECL_CAST( ptr, stream, t_ae_file_stream );
  int pos;
  int len;

  if( ptr->fptr == NULL ) return -1;

  /* compute the file's length by seeking to the end, getting the position,
   * and the seeking back to our original position.  The position at the end
   * of the file is the length of the file. */

  pos = ftell( ptr->fptr );
  fseek( ptr->fptr, 0, SEEK_END );
  len = ftell( ptr->fptr );
  fseek( ptr->fptr, pos, SEEK_SET );

  return len;
}

static int static_ae_file_stream_read( t_ae_stream stream, char* buffer, int length ) {
  DECL_CAST( ptr, stream, t_ae_file_stream );
  if( ptr->fptr == NULL ) return -1;
  return fread( buffer, 1, length, ptr->fptr );
}

static int static_ae_file_stream_close( t_ae_stream stream ) {
  DECL_CAST( ptr, stream, t_ae_file_stream );
  if( ptr->fptr == NULL ) return -1;
  if( !ptr->wrapped ) {
    fclose( ptr->fptr );
  }
  ptr->fptr = NULL;
  ptr->wrapped = 0;
  return 0;
}

static int static_ae_buffer_stream_get_length( t_ae_stream stream ) {
  DECL_CAST( ptr, stream, t_ae_buffer_stream );
  if( ptr->buffer == NULL ) return -1;
  return strlen( ptr->buffer );
}

static int static_ae_buffer_stream_read( t_ae_stream stream, char* buffer, int length ) {
  DECL_CAST( ptr, stream, t_ae_buffer_stream );
  int len;

  if( ptr->buffer == NULL ) return -1;
  len = strlen( ptr->buffer ) - ptr->pos;
  len = ( len > length ? length : len );
  memcpy( buffer, ptr->buffer+ptr->pos, len );

  return len;
}

static int static_ae_buffer_stream_close( t_ae_stream stream ) {
  DECL_CAST( ptr, stream, t_ae_buffer_stream );
  if( ptr->buffer == NULL ) return -1;
  free( ptr->buffer );
  ptr->buffer = NULL;
  ptr->pos = 0;
  return 0;
}

static t_ae_tag static_ae_comparison_tag( CONST char* name,
                                          int comparison )
{
  t_ae_comparison_tag* tag;

  tag = (t_ae_comparison_tag*)ae_typed_tag( name,
                                            static_ae_comparison_tag_process,
                                            sizeof( t_ae_comparison_tag ) );
  tag->comp_type = comparison;

  return (t_ae_tag)tag;
}

static int static_ae_replace_tag_apply( t_ae_tag tag,
                                        CONST char* text,
                                        t_ae_template_mgr mgr,
                                        FILE* output )
{
  DECL_CAST( tag_data, tag, t_ae_replace_tag );
  if( strcmp( tag_data->m_tag, text ) != 0 ) return 0;
  return tag_data->process( tag, text, mgr, output );
}

static int static_ae_replace_tag_process( t_ae_tag tag,
                                          CONST char* text,
                                          t_ae_template_mgr mgr,
                                          FILE* output )
{
  DECL_CAST( tag_data, tag, t_ae_replace_tag );
  if( tag_data->m_data != NULL ) {
    fputs( tag_data->m_data, output );
  }
  return 1;
}

static int static_ae_replace_tag_cleanup( t_ae_tag tag ) {
  DECL_CAST( tag_data, tag, t_ae_replace_tag );
  if( tag_data->m_data != NULL ) {
    free( tag_data->m_data );
  }
  tag_data->m_data = NULL;
  return 0;
}

static int static_ae_cyclical_replace_tag_process( t_ae_tag tag,
                                                   CONST char* text,
                                                   t_ae_template_mgr mgr,
                                                   FILE* output )
{
  DECL_CAST( tag_data, tag, t_ae_cyclical_replace_tag );
  char* end;

  if( *(tag_data->m_next) == 0 ) {
    fputs( "no more data values in cyclical replace tag", output );
    tag_data->m_next = 0;
    return 1;
  }

  /* find the next delimiter, in the data */
  end = strstr( tag_data->m_next, tag_data->m_rpt_delim );
  if( end == 0 ) {
    fputs( "non-terminated data string in cyclical replace tag", output );
    tag_data->m_next = 0;
    return 1;
  }

  /* write out all data up to the 'end' pointer */
  while( tag_data->m_next < end ) {
    fputc( *(tag_data->m_next), output );
    tag_data->m_next++;
  }

  /* move the pointer past the delimiter at the end of the record, so it now
   * points to the beginning of the next record. */

  tag_data->m_next += strlen( tag_data->m_rpt_delim );
  return 1;
}

static int static_ae_cyclical_replace_tag_cleanup( t_ae_tag tag ) {
  DECL_CAST( tag_data, tag, t_ae_cyclical_replace_tag );
  free( tag_data->m_data );
  free( tag_data->m_rpt_delim );
  tag_data->m_next = NULL;
  return 0;
}

static int static_ae_typed_tag_apply( t_ae_tag tag,
                                      CONST char* text,
                                      t_ae_template_mgr mgr,
                                      FILE* output )
{
  GENERIC_TAG( tag_data, tag );
  char* type;

  type = ae_get_field( text, tag_data->m_delim, 0 );
  if( type == NULL ) return 0;
  /* a typed tag matches if the first field of the text chunk matches the m_tag (name)
   * field of the tag.  If it does, then we call the process function for this tag. */
  if( ae_field_cmp( type, tag_data->m_tag, tag_data->m_delim ) != 0 ) return 0;
  return tag_data->process( tag, text, mgr, output );
}

static int static_ae_if_tag_process( t_ae_tag tag,
                                     CONST char* text,
                                     t_ae_template_mgr mgr,
                                     FILE* output )
{
  GENERIC_TAG( tag_data, tag );
  char* tok;
  char* value;
  char  tok_buf[ 64 ];

  tok = ae_get_field( text, tag_data->m_delim, 1 );
  ae_field_cpy( tok_buf, tok, tag_data->m_delim );
  value = ae_get_value( mgr, tok_buf );
  if( !( value && *value ) ) {
    return 0;
  }

  ae_process_buffer( mgr, ae_get_field( text, tag_data->m_delim, 2 ), output );
  return 1;
}

static int static_ae_if_not_tag_process( t_ae_tag tag,
                                         CONST char* text,
                                         t_ae_template_mgr mgr,
                                         FILE* output )
{
  GENERIC_TAG( tag_data, tag );
  char* tok;
  char* value;
  char  tok_buf[ 64 ];

  tok = ae_get_field( text, tag_data->m_delim, 1 );
  ae_field_cpy( tok_buf, tok, tag_data->m_delim );
  value = ae_get_value( mgr, tok_buf );
  if( value && *value ) {
    return 0;
  }

  ae_process_buffer( mgr, ae_get_field( text, tag_data->m_delim, 2 ), output );
  return 1;
}

static int static_ae_include_tag_process( t_ae_tag tag,
                                          CONST char* text,
                                          t_ae_template_mgr mgr,
                                          FILE* output )
{
  GENERIC_TAG( tag_data, tag );
  char* tok;

  tok = ae_get_field( text, tag_data->m_delim, 1 );
  if( ae_get_tag( mgr, tok ) != NULL ) {
    tok = ae_get_value( mgr, tok );
  }
  ae_process_template( mgr, tok, output );

  return 1;
}

#define ROW_NUM_TAG_NAME "ae_row_num"

static int static_ae_repeat_tag_process( t_ae_tag tag,
                                         CONST char* text,
                                         t_ae_template_mgr mgr,
                                         FILE* output )
{
  GENERIC_TAG( tag_data, tag );
  t_ae_cyclical_replace_tag* repl_tag;
  t_ae_replace_tag* row_tag;
  char* source;
  char* token;
  char* delim;
  char* data;
  char  row_num_tag[32];
  char  row_num_value[10];
  int   i;
  
  source = ae_get_field_alloc( text, tag_data->m_delim, 1 );
  token  = ae_get_field_alloc( text, tag_data->m_delim, 2 );
  delim  = ae_get_field_alloc( text, tag_data->m_delim, 3 );
  data   = ae_get_field( text, tag_data->m_delim, 4 );

  /* create a new cyclical replace tag from the delimited string associated with this
   * repeat tag.  Add it to the manager */

  repl_tag = ae_cyclical_replace_tag( token, ae_get_value( mgr, source ), delim );
  ae_add_tag_ex( mgr, repl_tag );

  /* look for the first available row_num tag name.  By default, we use ae_row_num, but
   * if it is taken then that means that we are currently embedded inside of a repeat tag.
   * So, we add a number to the end of the tag name and try again.  This continues until
   * a number is found that is not taken.  This allows repeat tags to be nested to an
   * arbitrary depth. */

  i = 1;
  strcpy( row_num_tag, ROW_NUM_TAG_NAME );
  while( ae_get_tag( mgr, row_num_tag ) != NULL ) {
    i++;
    sprintf( row_num_tag, ROW_NUM_TAG_NAME "_%d", i );
  }
  
  /* repeatedly process the data for the repeat tag, until the cyclical replace tag
   * is out of data.  Each pass through the data, we increment the row num and set
   * it in the row_num_tag variable. */

  i = 1;
  while( *(repl_tag->m_next) != 0 ) {
    sprintf( row_num_value, "%d", i );
    ae_add_tag( mgr, row_num_tag, row_num_value );
    ae_process_buffer( mgr, data, output );
    i++;
  }

  /* remove the row_num_tag and the cyclical replace tag from the manager */
  ae_remove_tag( mgr, row_num_tag );
  ae_remove_tag_ex( mgr, repl_tag );

  /* free our allocated data */
  free( source );
  free( token );
  free( delim );

  return 1;
}

static int static_ae_env_tag_process( t_ae_tag tag,
                                      CONST char* text,
                                      t_ae_template_mgr mgr,
                                      FILE* output )
{
  char *env;
  char* val;

  env = ae_get_field( text, ae_get_tag_delim( tag ), 1 );
  val = getenv( env );

  if( val ) {
    fputs( val, output );
  }

  return 1;
}

static int static_ae_exec_tag_process( t_ae_tag tag,
                                       CONST char* text,
                                       t_ae_template_mgr mgr,
                                       FILE* output )
{
  char *tok;
  char* val;
  FILE* pipe_output;
  char  buf[ 128 ];
  int   count;

  tok = ae_get_field( text, ae_get_tag_delim( tag ), 1 );
  if( ae_get_tag( mgr, tok ) != NULL ) {
    tok = ae_get_value( mgr, tok );
  }

  pipe_output = popen( tok, "r" );
  if( pipe_output == NULL ) {
    fprintf( output, "[popen failed: %d (%s)]", errno, strerror( errno ) );
  } else {
    while( ( count = fread( buf, 1, sizeof( buf )-1, pipe_output ) ) > 0 ) {
      buf[ count ] = 0;
      fputs( buf, output );
    }
    pclose( pipe_output );
  }

  return 1;
}

static int static_ae_comparison_tag_process( t_ae_tag tag,
                                             CONST char* text,
                                             t_ae_template_mgr mgr,
                                             FILE* output )
{
  DECL_CAST( tag_data, tag, t_ae_comparison_tag );
  char* tok_buf;
  char* val_buf;
  int   comp_result;

  tok_buf = ae_get_field_alloc( text, tag_data->m_delim, 1 );
  val_buf = ae_get_field_alloc( text, tag_data->m_delim, 2 );
  comp_result = strcmp( ae_get_value( mgr, tok_buf ), val_buf );

  switch( tag_data->comp_type ) {
    case COMP_TYPE_EQ: comp_result = ( comp_result == 0 ); break;
    case COMP_TYPE_NE: comp_result = ( comp_result != 0 ); break;
    case COMP_TYPE_LT: comp_result = ( comp_result < 0 ); break;
    case COMP_TYPE_LE: comp_result = ( comp_result <= 0 ); break;
    case COMP_TYPE_GT: comp_result = ( comp_result > 0 ); break;
    case COMP_TYPE_GE: comp_result = ( comp_result >= 0 ); break;
  }

  if( comp_result ) {
    ae_process_buffer( mgr, ae_get_field( text, tag_data->m_delim, 3 ), output );
  }

  free( val_buf );
  free( tok_buf );

  return 1;
}

static int static_ae_include_tag_named_process( t_ae_tag tag,
                                                CONST char* text,
                                                t_ae_template_mgr mgr,
                                                FILE* output )
{
  ae_process_template( mgr, ae_get_tag_value( tag ), output );
  return 1;
}

static int static_ae_shared_fn_apply( t_ae_tag tag,
                                      CONST char* text,
                                      t_ae_template_mgr mgr,
                                      FILE* output )
{
  GENERIC_TAG( tag_data, tag );

  if( ae_field_cmp( ae_get_field( text, tag_data->m_delim, 0 ), "EXEC_SHARED", tag_data->m_delim ) != 0 )
    return 0;

  if( ae_field_cmp( ae_get_field( text, tag_data->m_delim, 1 ), tag_data->m_tag, tag_data->m_delim ) != 0 )
    return 0;

  return tag_data->process( tag, text, mgr, output );
}

static int static_ae_shared_fn_process( t_ae_tag tag,
                                        CONST char* text,
                                        t_ae_template_mgr mgr,
                                        FILE* output )
{
  DECL_CAST( tag_data, tag, t_ae_shared_fn_tag );
  int (*func_ptr)( void* );
  char libname[ 256 ];

  ae_build_library_name( libname, tag_data->m_lib );
  func_ptr = (int(*)(void*))ae_load_dynamic_function( libname, tag_data->m_func );
  if( func_ptr == NULL ) return 1;

  /* flush the output, since the function may write to stdout.  Although stdout is
   * redirected to output already, if we don't flush the output here (and flush stdout
   * at the end of the function) we will get text written in the wrong order, due to
   * caching. */

  fflush( output );

  /* call the function */
  func_ptr( tag_data->m_cookie );
  fflush( stdout );

  return 1;
}

static int static_ae_shared_fn_tag_cleanup( t_ae_tag tag ) {
  DECL_CAST( tag_data, tag, t_ae_shared_fn_tag );
  free( tag_data->m_lib );
  free( tag_data->m_func );
  return 0;
}

static int static_html_preproc_fn( t_ae_template_mgr mgr, FILE* output ) {
  t_ae_html_proc_data* data;
  t_ae_cookie* cookie;
  struct tm *tstr;
  time_t curTime;
  char ttlBuffer[ 128 ];

  data = (t_ae_html_proc_data*)ae_get_mgr_cookie( mgr );

  if( data->headers ) {
    fputs( "Content-type: text/html\n", output );
    if( data->no_cache ) {
      fputs( "Pragma: no-cache\n", output );
      fputs( "Expires: Thu, 1 Jan 1970 00:00:01 GMT\n", output );
    }

    /* write the cookie definitions */
    cookie = data->cookies;
    while( cookie != NULL ) {
      fprintf( output, "Set-Cookie: %s=%s; PATH=/", cookie->name, cookie->value );

      if( cookie->ttl >= 0 ) {
        /* compute the time-to-live as the number of seconds from the current time.
         * Cookie TTL's must be given in GMT. */
        curTime = time( NULL ) + cookie->ttl;
        tstr = gmtime( &curTime );
        strftime( ttlBuffer, sizeof( ttlBuffer ), "%a, %d-%b-%y %H:%M:%S GMT", tstr );
        fprintf( output, "; EXPIRES=%s", ttlBuffer );
      }

      fprintf( output, "\n" );
      cookie = cookie->next;
    }

    fputs( "\n", output );
  }

  return 0;
}


static char* static_get_non_value( t_ae_tag tag ) {
  return NULL;
}

static char* static_get_replace_tag_value( t_ae_tag tag ) {
  DECL_CAST( tag_data, tag, t_ae_replace_tag );
  return tag_data->m_data;
}

