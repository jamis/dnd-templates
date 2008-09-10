/* ------------------------------------------------------------------------- *
 * Text Document Template Parser and Processor
 *
 * author:
 * Jamis Buck (jgb3@email.byu.edu)
 *
 * description:
 * The idea behind this module is that many dynamically presented documents
 * (like CGI pages) are basically the same with the exception of certain data
 * elements.  This module enables programs to define "templates" that contain
 * special tags which will be replaced with data at runtime.
 *
 * The concept of tags is extended by allowing tags to have more than simple
 * "replace" functionality.  The following tags are standard, and are
 * automatically defined by the template manager object:
 *
 *   IF=tok=data
 *   IF_NOT=tok=data
 *     If the tag 'tok' is not defined, or if it's value is NULL or the
 *     empty string, do (IF_NOT) or do not (IF) print the data.
 *   IF_EQ=tok=value=data
 *   IF_NOT_EQ=tok=value=data
 *     If the tag 'tok' is defined, and it's value is the same as 'value'
 *     (case sensitive string comparison), then do (IF_EQ) or do not
 *     (IF_NOT_EQ) print the data
 *   IF_LT=tok=value=data
 *   IF_LE=tok=value=data
 *   IF_GT=tok=value=data
 *   IF_GE=tok=value=data
 *     If the tag 'tok' is defined, and it's value is (<,<=,>,>=) 'value'
 *     (case sensitive string comparison), then print the data.
 *   INCLUDE=tok
 *     If the tag 'tok' exists, treat its value as a file-name, otherwise
 *     treat 'tok' as a file-name.  Parse the file's contents and place
 *     them at the location of the tag. (This calls ae_process_stream
 *     recursively).
 *   REPEAT2=source=newtok=delim=data
 *     If the tag 'source' exists, treat its value as a string of delimited
 *     items (where the delimiter is 'delim').  Create a cyclical replace
 *     tag from the delimited value.  Parse the 'data' repeatedly until there
 *     are no more values in the cyclical replace tag.  The 'newtok' is the
 *     name of the token that represents the cyclical replace tag within the
 *     REPEAT2 context.  Also, a token named "ae_row_num" is created to
 *     represent the (1-based) number of the current row.  If the repeat tag
 *     is nested within another repeat tag, the row-number tag will be named
 *     "ae_row_num_%" where '%' is the depth at which the repeat is nested
 *     within other repeats.
 *   ENV=env
 *     Write the value of the given environment variable.
 *   EXEC_SHARED=tok
 *     'tok' must indicate a shared_fn_tag, which instructs the engine to
 *     load a given shared library and execute a particular entry point.
 *     The entry point must take one void* parameter, which is specified
 *     when the tag is created.
 *   EXEC=tok
 *    'tok' must either name a replace tag, or be the name of a process
 *    to run, itself.  The process is then executed, and it's output
 *    inserted in the place of the tag.
 *
 * todo:
 * Error handling.  Currently, all errors are simply printed to stdout or
 * stderr.
 * ------------------------------------------------------------------------- */

#ifndef __TEMPLATES_H__
#define __TEMPLATES_H__

/* ------------------------------------------------------------------------- */
/* includes                                                                  */
/* ------------------------------------------------------------------------- */

#include <stdio.h> /* for FILE */

/* ------------------------------------------------------------------------- */
/* macro definitions                                                         */
/* ------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------- *
   * This #define is used to implement tags.  If your application wants to
   * create a custom tag, the structure for the tag must begin with these
   * fields.
   *
   * The 'type' field is one of TAG_TYPE_NO_VALUE or TAG_TYPE_VALUE, and
   * determines whether or not the get_value method has been implemented.
   *
   * The 'apply' method is used to determine if the tag can process the
   * given text.  If it can, it should call the process function for the tag
   * and return that function's return code, otherwise it should return 0.
   *
   * The 'process' method is used to process the text for a given tag.  If
   * the 'process' method does not or cannot handle the text, in spite of what
   * the 'apply' method assumed, this function should return 0, otherwise it
   * should return 1.
   *
   * The 'get_value' method is used to get the value for the given tag.  By
   * default, this returns NULL for all TAG_TYPE_NO_VALUE tags.
   *
   * The 'cleanup' method is used to clean up the memory allocated by the
   * given tag.  If it is null, no special cleanup is performed for the
   * tag.
   *
   * The 'm_tag' field is the name of the tag.
   *
   * The 'm_delim' field is the name of the string used to delimit the fields
   * of the token.
   * ----------------------------------------------------------------------- */

#define STANDARD_TAG_HDR \
  int type; \
  t_ae_tag_fn apply; \
  t_ae_tag_fn process; \
  t_ae_tag_get_fn get_value; \
  int (*cleanup)( t_ae_tag ); \
  char* m_tag; \
  char* m_delim

  /* ----------------------------------------------------------------------- *
   * This #define is used to implement streams.  Every stream record begins
   * with and must implement these fields.  Developers may implement
   * custom stream objects by creating structures that conform to this
   * signature.
   * ----------------------------------------------------------------------- */

#define STANDARD_STREAM_HDR \
  int (*get_length)( t_ae_stream ); \
  int (*read)( t_ae_stream, char*, int ); \
  int (*close)( t_ae_stream )

  /* ----------------------------------------------------------------------- *
   * For HTML parsing, these defines are used with the ToHTML2 function,
   * and define what kind of headers to print out before parsing the stream.
   * ----------------------------------------------------------------------- */

#define HTML_HEADER         ( 0x01 )
#define HTML_NO_CACHE       ( 0x02 )

  /* ----------------------------------------------------------------------- *
   * Some tags have values, others (like BYU_IF) don't.  If a tag is of type
   * TAG_TYPE_VALUE, then it implements it's 'get_value' function.
   * ----------------------------------------------------------------------- */

#define TAG_TYPE_NO_VALUE   (    0 )
#define TAG_TYPE_VALUE      (    1 )

#define CONST const

/* ------------------------------------------------------------------------- */
/* type definitions                                                          */
/* ------------------------------------------------------------------------- */

typedef void*    t_ae_stream;
typedef void*    t_ae_template_mgr;
typedef void*    t_ae_tag;

typedef int (*t_ae_tag_fn)( t_ae_tag, CONST char*, t_ae_template_mgr, FILE* );
typedef char* (*t_ae_tag_get_fn)( t_ae_tag );
typedef int (*t_ae_preproc_fn)( t_ae_template_mgr, FILE* );

typedef struct {
  STANDARD_TAG_HDR;
} t_ae_generic_tag;

/* ------------------------------------------------------------------------- */
/* stream manipulation functions                                             */
/* ------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------- *
   * Create a new stream (read-only) by file-name, file-pointer, or buffer.
   * File streams read from the indicated file, and buffer streams read from
   * the indicated buffer.  If the stream could not be opened, the return
   * value is NULL.
   * ----------------------------------------------------------------------- */
t_ae_stream ae_stream_open_file( CONST char* file_name );
t_ae_stream ae_stream_wrap_file( FILE* fptr );
t_ae_stream ae_stream_open_buffer( CONST char* buffer );

  /* ----------------------------------------------------------------------- *
   * Operate on the stream by obtaining the length of the stream, or by
   * reading a given number of bytes from the stream.  For ae_stream_read,
   * if there are not that many bytes in the stream, all available bytes
   * will be read.  The return code is the number of bytes read, or 0 if
   * the end of the stream was reached before reading any bytes.
   * ----------------------------------------------------------------------- */
int         ae_stream_get_length( t_ae_stream stream );
int         ae_stream_read( t_ae_stream stream, char* buffer, int length );

  /* ----------------------------------------------------------------------- *
   * Close and deallocate the given stream object.
   * ----------------------------------------------------------------------- */
int         ae_stream_close( t_ae_stream stream );

/* ------------------------------------------------------------------------- */
/* template manipulation functions                                           */
/* ------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------- *
   * Create or destroy a template manager object.  Creating a template
   * manager object automatically adds the IF, IF_NOT, IF_EQ,
   * IF_NOT_EQ, INCLUDE, REPEAT2, and ENV tokens to the manager.
   * Destroying a template manager object also destroys all associated
   * tokens.
   * ----------------------------------------------------------------------- */
t_ae_template_mgr ae_template_mgr_new( void );
void              ae_template_mgr_done( t_ae_template_mgr mgr );

  /* ----------------------------------------------------------------------- *
   * Add a tag to the template manager.  ae_add_tag adds a new replace token
   * to the manager with the given name and value.  ae_add_tag_ex adds the
   * given token to the manager.  If a token already exists in the manager
   * with the given name (or tag's name), the existing token is removed and
   * destroyed and the new token is added.
   *
   * ae_add_tags adds a list of name/value pairs to the manager.  The names
   * list must be null-terminated to indicate the end of the list.  If any
   * name begins with a '-' character, the value is assumed to be a tag
   * object, and not a string, and will be added directly to the manager.
   *
   * Adding a tag to the template manager sets that tag's delimiter to the
   * delimiter defined by the template manager.
   * ----------------------------------------------------------------------- */
void              ae_add_tag( t_ae_template_mgr mgr, CONST char* name, CONST char* value );
void              ae_add_tag_i( t_ae_template_mgr mgr, CONST char* name, int value );
void              ae_add_tag_ex( t_ae_template_mgr mgr, t_ae_tag tag );
void              ae_add_tags( t_ae_template_mgr mgr, char** names, char** values );

  /* ----------------------------------------------------------------------- *
   * Removes a tag from the manager, either by name or by reference.  The
   * tag is destroyed and deallocated.
   * ----------------------------------------------------------------------- */
void              ae_remove_tag( t_ae_template_mgr mgr, CONST char* name );
void              ae_remove_tag_ex( t_ae_template_mgr mgr, t_ae_tag tag );

  /* ----------------------------------------------------------------------- *
   * Returns the number of tags in the manager.
   * ----------------------------------------------------------------------- */
int               ae_tag_count( t_ae_template_mgr mgr );

  /* ----------------------------------------------------------------------- *
   * Change the delimiters that surround the tags.
   * ----------------------------------------------------------------------- */
void              ae_set_start_end_delim( t_ae_template_mgr mgr,
                                          CONST char* start,
                                          CONST char* end );

  /* ----------------------------------------------------------------------- *
   * Returns the tag object that answers to the given name.  Tag names are
   * case sensitive, so be sure the case is the same as the tag you are
   * wanting to retrieve.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_get_tag( t_ae_template_mgr mgr, CONST char* name );

  /* ----------------------------------------------------------------------- *
   * Returns the value of the tag with the given name.
   * ----------------------------------------------------------------------- */
char*             ae_get_value( t_ae_template_mgr mgr, CONST char* name );

  /* ----------------------------------------------------------------------- *
   * Returns the tag object at the given index in the template manager.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_get_tag_at( t_ae_template_mgr mgr, int index );

  /* ----------------------------------------------------------------------- *
   * Process a stream, buffer, or file.  This will parse the given stream,
   * writing all output to the 'output' stream.  Tokens in the manager are
   * examined and if no token is found that answers to a particular token
   * in the stream, it is printed as found.  The first tag that answers to
   * a particular token will be allowed to process that token.
   * ----------------------------------------------------------------------- */
int ae_process_template( t_ae_template_mgr mgr, CONST char* file, FILE* output );
int ae_process_buffer( t_ae_template_mgr mgr, CONST char* buffer, FILE* output );
int ae_process_stream( t_ae_template_mgr mgr, t_ae_stream stream, FILE* output ); 

  /* ----------------------------------------------------------------------- *
   * The preprocessor function, if set, is called prior to any template
   * processing when any of the ae_process_xxx functions are called.  The
   * function will only be called if the template manager has not been called
   * recursively.
   *
   * The cookie is an arbitrary application-defined value that may be passed
   * via the template manager.  This allows custom tokens to access
   * application-specific data without the need for global variables.
   * ----------------------------------------------------------------------- */
void  ae_set_preprocessor_func( t_ae_template_mgr mgr, t_ae_preproc_fn func );
void  ae_set_mgr_cookie( t_ae_template_mgr mgr, void* cookie );
void* ae_get_mgr_cookie( t_ae_template_mgr mgr );

/* ------------------------------------------------------------------------- */
/* tag manipulation functions                                                */
/* ------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------- *
   * ae_tag_new creates a new generic tag of the given size.  'size' must
   * be at least the size of the t_ae_generic_tag structure, or you will have
   * memory leaks.  The new tag is initialized with the given tag name and
   * a default delimiter.
   *
   * ae_tag_destroy destroys the given tag and calls it's 'cleanup' method,
   * if defined.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_tag_new( CONST char* name, int size );
void              ae_tag_destroy( t_ae_tag tag );

  /* ----------------------------------------------------------------------- *
   * Returns the name, delimiter, and value for the given tag.
   * ----------------------------------------------------------------------- */
char*             ae_get_tag_name( t_ae_tag tag );
char*             ae_get_tag_delim( t_ae_tag tag );
char*             ae_get_tag_value( t_ae_tag tag );

  /* ----------------------------------------------------------------------- *
   * Create a new replace tag with the given name and data.  Any time a token
   * with the given name is found, it will be replaced with the given data.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_replace_tag( CONST char* name, CONST char* data );

  /* ----------------------------------------------------------------------- *
   * Create a new cyclical replace tag with the given name and data.  This
   * is used primarily internally by the repeat tag, but may have other
   * uses.  'data' should be a string of data delimited by 'delim'.  Each
   * time the 'apply' method of the tag is called, the next element in the
   * data list is printed.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_cyclical_replace_tag( CONST char* name, CONST char* data, CONST char* delim );

  /* ----------------------------------------------------------------------- *
   * This function is used internally to create a generic "typed" tag,
   * that is to say, a tag like "IF", "INCLUDE", or "REPEAT2", rather than
   * a replace tag.  The 'process' parameter is a pointer to the function
   * that will be called to process this tag when it is applied.  The 'size'
   * parameter is the size of the tag structure to allocate. Unless you need
   * a custom structure, simply set this parameter to sizeof( t_ae_generic_tag ).
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_typed_tag( CONST char* type, t_ae_tag_fn process, int size );

  /* ----------------------------------------------------------------------- *
   * Creates a new tag of the appropriate type.  Since these tags are
   * created automatically when a template manager is created, there is
   * rarely a need to create one explicitly.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_if_tag( void );
t_ae_tag          ae_if_not_tag( void );
t_ae_tag          ae_if_eq_tag( void );
t_ae_tag          ae_if_not_eq_tag( void );
t_ae_tag          ae_if_lt_tag( void );
t_ae_tag          ae_if_le_tag( void );
t_ae_tag          ae_if_gt_tag( void );
t_ae_tag          ae_if_ge_tag( void );
t_ae_tag          ae_include_tag( void );
t_ae_tag          ae_repeat_tag( void );
t_ae_tag          ae_env_tag( void );
t_ae_tag          ae_exec_tag( void );

  /* ----------------------------------------------------------------------- *
   * Create a new tag that will load the given library and execute the given
   * function.  The library name given should be 'bare', meaning that it
   * should contain no path information, no file suffix, and no lib prefix.
   * The actual library name will be constructed in a platform-specific
   * manner from the given name.  The given 'cookie' will be passed to the
   * function function when it is called.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_shared_fn_tag( CONST char* name, CONST char* lib,
                                    CONST char* func, void* cookie );

  /* ----------------------------------------------------------------------- *
   * As with ae_shared_fn_tag, but it uses 'name' as the name of the tag,
   * instead of EXEC_SHARED.  This means the tag will look identical to
   * a replace tag, but will behave like an EXEC_SHARED tag.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_shared_fn_tag_named( CONST char* name, CONST char* lib,
                                          CONST char* func, void* cookie );

  /* ----------------------------------------------------------------------- *
   * As with ae_include_tag, but it uses 'name' as the name of the tag,
   * instead of INCLUDE.  This means the tag will look identical to
   * a replace tag, but will behave like an INCLUDE tag.
   * ----------------------------------------------------------------------- */
t_ae_tag          ae_include_tag_named( CONST char* name, CONST char* file );

/* ------------------------------------------------------------------------- */
/* ToHTML Replacement Functions                                              */
/* ------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------- *
   * Creates a new template manager, populates it with the given tokens
   * (using ae_add_tags), prints headers depending on the value of 'mode',
   * and calls ae_process_template on the given tem_file name.
   * ----------------------------------------------------------------------- */
int ToHTML2( CONST char* tem_file, char** tokens, char** values, int mode );

  /* ----------------------------------------------------------------------- *
   * This pair of functions is used internally by the ToHTML2 function, but
   * may be of use to developers who wish the ease of ToHTML2 with some of
   * the flexibility of accessing the template API functions directly.
   *
   * ae_init_html creates a new template manager object, initializes it with the
   * given tags (and according to the given mode) and then returns the handle
   * to the manager.
   *
   * ae_done_html process the given template file and destroys the template
   * manager object.  If the tem_file is NULL, no template is processed,
   * but the manager object is still destroyed.
   *
   * ToHTML2( x, y, z, w ) is the same as:
   *   ae_done_html( ae_init_html( y, z, w ), x )
   * ----------------------------------------------------------------------- */
t_ae_template_mgr ae_init_html( char** tokens, char** values, int mode );
int               ae_done_html( t_ae_template_mgr mgr, CONST char* tem_file );

  /* ----------------------------------------------------------------------- *
   * This routine sets the given name=value pair (with the given time-to-live,
   * in seconds), to be written as a cookie when the next HTML template is
   * written.  This routine requires that the 'mgr' handle be one that was
   * returned from 'ae_init_html'.  The cookies will only be written if
   * HTML_HEADER is one of the 'mode' values passed to ae_init_html.
   * ----------------------------------------------------------------------- */
void              ae_set_cookie( t_ae_template_mgr mgr,
                                 CONST char* name,
                                 CONST char* value,
                                 int ttl );

  /* ----------------------------------------------------------------------- *
   * This routine sets the output stream to be written to by ae_done_html.
   * By default, ae_done_html writes to stdout.
   * ----------------------------------------------------------------------- */
void              ae_set_html_output( t_ae_template_mgr mgr,
                                      FILE* output );

/* ------------------------------------------------------------------------- */
/* miscellaneous utility functions                                           */
/* ------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------- *
   * These functions are provided for the convenience of developers of
   * custom tags, in working with the fields within a tag.
   *
   * ae_get_field returns a non-null-terminated string representing the given
   * field of the given string, with the given delimiter.  This string should
   * never be freed.
   *
   * ae_get_field_alloc returns a new null-terminated string that MUST be
   * freed by the calling application.
   *
   * ae_field_cmp compares the given field (terminated either by a NULL or
   * by the given delimiter) with the given null-terminated text, as with
   * strcmp.
   *
   * ae_field_len returns the number of bytes in field up to the first
   * NULL byte, or the given delimiter.
   *
   * ae_field_cpy copies field (up to a NULL byte, or the given delimiter)
   * into dest.
   * ----------------------------------------------------------------------- */
char* ae_get_field( CONST char* text, CONST char* delim, int which );
char* ae_get_field_alloc( CONST char* text, CONST char* delim, int which );

int   ae_field_cmp( CONST char* field, CONST char* text, CONST char* delim );
int   ae_field_len( CONST char* field, CONST char* delim );
char* ae_field_cpy( char* dest, CONST char* field, CONST char* delim );

  /* ----------------------------------------------------------------------- *
   * This function loads the given library (which must be either a fully
   * qualified path name, or the name of a library in LD_LIBRARY_PATH) and
   * returns the function with the given name.  If the function could not
   * be found, or the library could not be loaded, returns NULL.
   * ----------------------------------------------------------------------- */
void* ae_load_dynamic_function( CONST char* lib, CONST char* func );

  /* ----------------------------------------------------------------------- *
   * From the given root, builds the name of a shared library in a platform
   * dependant manner.  If 'dest' is non-NULL, the name will be copied into
   * 'dest' and 'dest' will be returned.  If 'dest' is NULL, a new buffer
   * will be allocated and returned, which must be freed by the calling
   * routine.
   * ----------------------------------------------------------------------- */
char* ae_build_library_name( char* dest, CONST char* root );

  /* ----------------------------------------------------------------------- *
   * The following two routines are for redirecting a file to a
   * different file, and later restoring the file to its original destination.
   *
   * ae_redirect_to returns a file descriptor that corresponds to the
   * "original" value of 'original_fd'.  Anything written to it will be
   * written to what 'original_fd' was originally writing to, but anything
   * written to original_fd after a call to this function will be written to
   * 'output' instead. This function returns a -1 if 'output' and
   * 'original_fd' are the same.
   *
   * ae_restore_file makes 'file' point to a copy of the given file-descriptor,
   * and then closes 'original_fd'.  Note that if 'original_fd' is negative,
   * the function does nothing (and so is safe to use no matter what
   * ae_redirect_to returns).
   * ----------------------------------------------------------------------- */
int   ae_redirect_to( FILE* output, int original_fd );
void  ae_restore_file( int original_fd, FILE* file );

#endif /* __TEMPLATES_H__ */

