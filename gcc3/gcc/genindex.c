/* APPLE LOCAL file indexing */
/* genindex.c Indexing information is used by the IDEs like Project Builder.
   All the functions related to the indexing information generation. 

   This file is used by both - preprocessor (cpp) and the compiler (cc1*)
*/

#include "config.h"
#include "system.h"
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

/* Socket is used to put indexing information.  */
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

#include "genindex.h"

extern int flag_cpp_precomp;
extern void warning PARAMS ((const char *, ...));

enum index_language_kind index_language = PB_INDEX_LANGUAGE_INVALID;

#define DEBUG_INDEXING 1

#define MAX_INDEX_FILENAME_SIZE 1024

/* Flag to indicate, if indexing information needs to be generated */
int flag_debug_gen_index = 0;    /* use std out */
int flag_gen_index = 0;          /* use socket */
int flag_gen_index_original = 0; /* use socket */
int flag_gen_index_header = 0;   /* Generate index header */
int flag_suppress_builtin_indexing = 0; /* Suppress indexing info for
                                           the builtin symbols.  */

/* Counter to keep track of already included headers.
   This counter is used to decide if indexing information generation.
   If the counter is zero (initial value) than only generate indexing 
   information.  Whenever a header, whose indexing information is already
   generated, is seen this counter is incremented by one on entry and 
   decremented by one on exit.  */
static int skip_index_generation = 0;

/* Counter to remember the depth of recursion. Initialized as zero, 
   this counter is incremented by one on entry of recursivly included 
   header and decremented by one on exit. If counter is zero then only 
   generate indexing information otherwise recursively included header 
   is being processed and avoid generating indexing info.  */
static int recursion_depth = 0;

/* Dirty bit. If this bit is set then only write the list into the file.  */
static int indexed_header_list_dirty = 0;
/* Use sockets to output indexing information.  */
static int index_socket_fd = -1;        /* Socket descriptor */
static char *index_host_name = 0;       /* Hostname, used for indexing */
static char *index_port_string = 0;     /* Port, used for indexing */
static unsigned index_port_number = 0;  /* Port, used for indexing */

/* Previous header name, for which we started index generation.
   This name is used, when end of header is encountered to
   update the status.
   End of header name provided by cpp-precomp is not accurate
   hence use this workaround. 
*/
static int MAX_BEGIN_COUNT = 4096;
int begin_header_count = 0;
char **begin_header_stack = NULL; 

/* Indexed header list.  */
int flag_check_indexed_header_list = 0;
char *index_header_list_filename = 0;
struct indexed_header
{
  struct indexed_header *next;
  char *name;
  time_t timestamp;    /* time of last data modification. */

  int timestamp_status;/* Flag to indicate valid timestamp for the 
                          header.  This is used to avoid deleting a 
                          node from the list.  Instead during the 
                          final write do not write invalid marked 
                          nodes.  */
  int status;          /* Status to indicate, if this header has 
                          been processed, it is not processed or 
                          it is being processed.  */
};

enum {
  INDEX_TIMESTAMP_INVALID,       /* Timestamp is invalid. Do not write
                                    this entry into the list file.  */
  INDEX_TIMESTAMP_NOT_VALIDATED, /* Timestamp is not validated yet. It
                                    may not be validated during this 
                                    compilation, but it is not invalid 
                                    so write this entry into the list
                                    file, if required.  */
  INDEX_TIMESTAMP_VALID          /* Valid timestamp */
};

static struct indexed_header *indexed_header_list = NULL;


/* Static function prototypes */
static void allocate_begin_header_stack   PARAMS ((void));
static void reallocate_begin_header_stack PARAMS ((void));
static void push_begin_header_stack       PARAMS ((char *));
static char * pop_begin_header_stack      PARAMS ((void));
static void maybe_flush_index_buffer      PARAMS ((int));
static void allocate_index_buffer         PARAMS ((void));
static char * absolute_path_name          PARAMS ((char *));

/* Establish socket connection to put the indexing information.  */
int 
connect_to_socket (hostname, port_number)
    char *hostname;
    unsigned port_number;
{
  int socket_fd;
  struct sockaddr_in addr;
   
  memset ((char *)&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (hostname == NULL) ? 
                         INADDR_LOOPBACK : 
                         inet_addr (hostname);
  addr.sin_port = htons (port_number);
    
  socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_fd < 0)
    {
       warning("cannot create socket: %s", strerror (errno));
       return -1;
    }
  if (connect (socket_fd, (struct sockaddr *)&addr, sizeof (addr)) < 0)
    {
       warning("cannot connect to socket: %s", strerror (errno));
       return -1;
    }
  return socket_fd;
}

/* Initialize socket communication channels.  */
void
init_gen_indexing ()
{
#if 0
  /* Turn OFF indexing, until further testing with new PB is done.  */
  flag_gen_index = 0;
  flag_gen_index_original = 0;
  return;
#endif

  if (flag_debug_gen_index)
    {
      flag_gen_index = 1;
      flag_gen_index_original  = 1;
      flag_gen_index_header = 1;
      return;
    }

  /* Check the environment variable for indexing */
  index_port_string = getenv ("PB_INDEX_SOCKET_PORT");
  if (index_port_string && *index_port_string)
    {       
      index_port_number = atoi (index_port_string);
      flag_gen_index  = 1;
      flag_gen_index_original  = 1;
      flag_gen_index_header = 1;

      index_host_name = getenv ("PB_INDEX_SOCKET_HOSTNAME");
      if (index_host_name && *index_host_name)
        {  /* keep the host name */ }
      else    
        index_host_name = NULL; 
    }       

  if (flag_gen_index)
    {
      /* See if the list of indexed header file is to be checked 
         before generating index information for  the headers.  */
      index_header_list_filename = getenv ("PB_INDEXED_HEADERS_FILE");
      if (index_header_list_filename && *index_header_list_filename)
        {       
          read_indexed_header_list ();
          /* Irrespective of if indexed_header file is present or not, 
             switch ON indexed_header list.  */
          flag_check_indexed_header_list = 1;
        }

      /* open socket for communication */
      index_socket_fd = connect_to_socket (index_host_name,
                                           index_port_number);
      if (index_socket_fd == -1)
        {
          warning ("Indexing information is not produced.");
	  flag_gen_index = 0;
        }
    }
}

/* Finalize index generation */ 
void
finish_gen_indexing ()
{
  if (!flag_debug_gen_index)
    if (close (index_socket_fd) < 0)
      warning ("cannot close the indexing data socket");
}

char *index_buffer = NULL;
int index_buffer_count = 0;
#define INDEX_BUFFER_SIZE 16000 

/* Allocate memory for the index buffer if required.  */
static void
allocate_index_buffer ()
{
  if (index_buffer == NULL)
    index_buffer = (char *) xcalloc (INDEX_BUFFER_SIZE, sizeof (char));
}

/* flush index buffer */
void
flush_index_buffer ()
{
   int l = 0;

   if (flag_debug_gen_index)
     fprintf (stderr, "%s", index_buffer);
   else
     {
       l = write (index_socket_fd, (void *)index_buffer, strlen (index_buffer));
       if (l < (int) strlen (index_buffer)) 
         warning("Indexing socket communication error.\n");
     }
   free (index_buffer);
   index_buffer_count = 0;
   index_buffer = NULL;
}

/* If required, 
    - flush index buffer and write info onto the socket   
    - Allocate new buffer 
*/
static void
maybe_flush_index_buffer (int i)
{
  if ((index_buffer_count + i) > (INDEX_BUFFER_SIZE  - 20))
    flush_index_buffer();

  allocate_index_buffer ();
}

/* Generate the indexing information and pass it through socket.  */
void
gen_indexing_info (info_tag, name, number)
    int info_tag; /* symbol information */
    const char *name; /* name of the symbol */
    int  number; /* line number */
{
  int extra_length = 20;  /* Max. for line no + ~ + operator + end-of-line */ 
  int info_length = 0;
  int name_length = 0;
  char nbuf[12];
  char info[6];

  /* Suppress indexing info generation for builtin symbols.  */
  if (flag_suppress_builtin_indexing)
    return;

  /* strcpy (info, "    ");  */
  switch (info_tag)
  {
    case INDEX_ENUM:
    case INDEX_VAR_DECL:
      strcpy (info, "+vm ");
      break;
    case INDEX_FILE_BEGIN:
      /* If cpp-precomp is used, then it's File Resume.  */
      if (flag_cpp_precomp)
        strcpy (info, "<Fm ");
      else
        strcpy (info, "+Fm ");
      break;
    case INDEX_FILE_INCLUDE:
      strcpy (info, "+Fi ");
      break;
    case INDEX_FILE_END:
      strcpy (info, "-Fm ");
      break;
    case INDEX_FUNCTION_BEGIN:
      strcpy (info, "+fm ");
      break;
    case INDEX_FUNCTION_END:
      strcpy (info, "-fm ");
      break;
    case INDEX_FUNCTION_DECL:
      strcpy (info, "+fh ");
      break;
    case INDEX_CONST_DECL:
      strcpy (info, "+nh ");
      break;
    case INDEX_TYPE_DECL:
      strcpy (info, "+th ");
      break;
    case INDEX_PROTOCOL_BEGIN:
      strcpy (info, "+Pm ");
      break;
    case INDEX_PROTOCOL_END:
      strcpy (info, "-Pm ");
      break;
    case INDEX_PROTOCOL_INHERITANCE:
      strcpy (info, "+Pi ");
      break;
    case INDEX_CATEGORY_BEGIN:
      strcpy (info, "+Cm ");
      break;
    case INDEX_CATEGORY_END:
      strcpy (info, "-Cm ");
      break;
    case INDEX_CATEGORY_DECL:
      strcpy (info, "+Ch ");
      break;
    case INDEX_CATEGORY_DECL_END:
      strcpy (info, "-Ch ");
      break;
    case INDEX_CLASS_METHOD_BEGIN:
    case INDEX_CLASS_OPERATOR_BEGIN:
      strcpy (info, "++m ");
      break;
    case INDEX_CLASS_METHOD_END:
      strcpy (info, "-+m ");
      break;
    case INDEX_CLASS_METHOD_DECL:
    case INDEX_CLASS_OPERATOR_DECL:
      strcpy (info, "++h ");
      break;
    case INDEX_INSTANCE_METHOD_BEGIN:
    case INDEX_INSTANCE_OPERATOR_BEGIN:
    case INDEX_CONSTRUCTOR_BEGIN:
      strcpy (info, "+-m ");
      break;
    case INDEX_INSTANCE_METHOD_END:
      strcpy (info, "--m ");
      break;
    case INDEX_INSTANCE_METHOD_DECL:
    case INDEX_INSTANCE_OPERATOR_DECL:
    case INDEX_CONSTRUCTOR_DECL:
      strcpy (info, "+-h ");
      break;
    case INDEX_DESTRUCTOR_BEGIN:
      strcpy (info, "+-m ");
      break;
    case INDEX_DESTRUCTOR_DECL:
      strcpy (info, "+-h ");
      break;
    case INDEX_MACRO:
      strcpy (info, "+Mh ");
      break;
    case INDEX_DATA_DECL:
      strcpy (info, "+dh ");
      break;
    case INDEX_DATA_INHERITANCE:
      strcpy (info, "+di ");
      break;
    case INDEX_NAMESPACE_DECL:
      strcpy (info, "+Nh ");
      break;
    case INDEX_CLASS_DECL:
      strcpy (info, "+ch ");
      break;
    case INDEX_CLASS_DECL_END:
      strcpy (info, "-ch ");
      break;
    case INDEX_CLASS_BEGIN:
      strcpy (info, "+cm ");
      break;
    case INDEX_CLASS_END:
      strcpy (info, "-cm ");
      break;
    case INDEX_CLASS_INHERITANCE:
      strcpy (info, "+ci ");
      break;
    case INDEX_RECORD_BEGIN:
      strcpy (info, "+sm ");
      break;
    case INDEX_RECORD_END:
      strcpy (info, "-sm ");
      break;
    case INDEX_UNION_BEGIN:
      strcpy (info, "+um ");
      break;
    case INDEX_UNION_END:
      strcpy (info, "-um ");
      break;
    case INDEX_ERROR:
    default:
      fprintf (stderr,"indexing error: invalid info_tag\n");
      return;
      break;
  }

  if (info)
      info_length = strlen (info);
  if (name)
      name_length = strlen (name);
  maybe_flush_index_buffer (info_length + name_length + extra_length);

  if (info)
    {
      strcat (index_buffer, info);
      index_buffer_count += info_length;
      if (index_language != PB_INDEX_LANGUAGE_INVALID)
        {
          sprintf(&nbuf[0],"%d ", index_language); 
          strcat (index_buffer,  nbuf);
          index_buffer_count += strlen (nbuf);
        }
    }

  if (number != -1)
    {
      sprintf (&nbuf[0],"%u ", number);
      strcat (index_buffer,  nbuf);
      index_buffer_count += strlen (nbuf);
    }

  if (name)
    {
      /* Fix symbol name if required.  */
      if (info_tag == INDEX_DESTRUCTOR_BEGIN 
	  || info_tag == INDEX_DESTRUCTOR_DECL)
      {
        memcpy(&index_buffer[index_buffer_count], "~", 1);
        index_buffer_count++;
        index_buffer [index_buffer_count] = NULL;
      }
      if (info_tag == INDEX_INSTANCE_OPERATOR_DECL
	  || info_tag == INDEX_CLASS_OPERATOR_DECL
	  || info_tag == INDEX_INSTANCE_OPERATOR_BEGIN
	  || info_tag == INDEX_CLASS_OPERATOR_BEGIN)
      {
        memcpy(&index_buffer[index_buffer_count], "operator ", 9);
        index_buffer_count += 9;
        index_buffer [index_buffer_count] = NULL;
      }
      strcat (index_buffer,  name);
      index_buffer_count += name_length;
    }
 
  /* end of line */
  memcpy(&index_buffer[index_buffer_count], "\n", 1);
  index_buffer_count++;
  index_buffer [index_buffer_count] = NULL;
}

/* Generate indexing header.
   Indexing header includes following info:
   marker : pbxindex-begin
   version number:  v1.2
   process id
   Component number : 2 if cpp-precomp is used, otherwise 1
   Total no. of Components : 2 if cpp-precomp is used, otherwise 1
   name: Full name of the input source file.
*/
void gen_indexing_header (name)
     char *name;
{
  char *header;
  int len = strlen (name);
  int components;

  /* If cpp-precomp is used as a preprocessor, then we have two
     components.  */
  if (flag_cpp_precomp)
    components = 2;
  else
    components = 1;

  header = (char *) xmalloc (sizeof (char) * (40 + len));
  sprintf (header, "pbxindex-begin v1.2 0x%08lX %02u/%02u %s\n",
	   (unsigned long) getppid(), components, components, name);
  maybe_flush_index_buffer (len);
  strcat (index_buffer, header);
  index_buffer_count += strlen (header);
  free (header);
}

void gen_indexing_footer ()
{
  const char *footer = "pbxindex-end ?\n";
  int len = strlen (footer);

  maybe_flush_index_buffer (len);
  strcat (index_buffer, footer);
  index_buffer_count += len;
}

/* Read the list of headers already indexed from the
  'index_header_list_filename' file.  */
int
read_indexed_header_list ()
{
  char buffer[MAX_INDEX_FILENAME_SIZE];
  struct indexed_header *cursor;
  FILE *file;
  char *name = NULL;
  time_t timestamp = 0;
  int i, length = 0;

  file = fopen (index_header_list_filename, "r");
  if (!file)
     return 0;

  while (fgets (buffer, MAX_INDEX_FILENAME_SIZE, file))
    {
      length = strlen (buffer);
      while (length > 0 && buffer [length - 1] <= ' ')
        buffer [-- length] = '\0';

      /* Separate the timestamp.  */ 
      for (i = 0; i < length; i++)
         {
            if (buffer [i] == ' ')
              {
                name = &buffer [i+1];
                buffer [i] = NULL;
                timestamp = atol (buffer);
                break;
              }
         }
 
       /* create new indexed_header with the name and timestamp.  */
      cursor = add_index_header (name, timestamp);
      if (cursor)
        {
          /* Initialized the header as already been processed.  */
          cursor->status = PB_INDEX_DONE;
          cursor->timestamp_status = INDEX_TIMESTAMP_NOT_VALIDATED;
        }
    }

  /* Clear dirty bit. List is not dirty, because we just
     read it from the file.  */
  indexed_header_list_dirty = 0;

  fclose (file);
  return 1;
}

/* Write the index header list into the 'index_header_list_filename' file.  */
void
write_indexed_header_list ()
{
  FILE *file;
  struct indexed_header *cursor;

  if (indexed_header_list_dirty == 0)
    return;  /* Nothing to write */

  file = fopen (index_header_list_filename, "w");
  if (!file)
    return;

  for (cursor = indexed_header_list; 
       cursor != NULL; 
       cursor = cursor->next)
    {
      if (cursor->timestamp_status != INDEX_TIMESTAMP_INVALID 
					&& cursor->status == PB_INDEX_DONE)
        fprintf (file, "%ld %s\n", cursor->timestamp, cursor->name);
    }
  fclose (file);
}

/* Add the name into the indexed header list.  
   Get the last modification time for the given name.  */
struct indexed_header *
add_index_header_name (str)
     char *str;
{
  struct stat buf;
  if (!str)
    return NULL;

  /* Get the file modification time.  */
  if (stat(str, &buf)) 
    return NULL;

  return add_index_header (str, buf.st_mtime);
}

/* Add the indexed header list.  */
struct indexed_header *
add_index_header (str, timestamp)
     char *str;
     time_t timestamp;
{
  struct indexed_header *h;

  /* Allocate memory and copy data.  */
  h = (struct indexed_header *) xmalloc (sizeof (struct indexed_header));
  if (!h)
    return NULL;

  h->name = (char *) xmalloc (sizeof (char) * (strlen (str) + 1));
  if (!h->name)
    return NULL;
  
  /* Get the values.  */
  strcpy (h->name,str);
  h->timestamp = timestamp;

  /* Add in front of the list. */
  h->next = indexed_header_list;
  indexed_header_list = h;

  /* Now the list is dirty.  */
  indexed_header_list_dirty = 1;
  return h;
}

void
print_indexed_header_list ()
{

  int count = 0;
  struct indexed_header *cursor;

  for (cursor = indexed_header_list; cursor != NULL; cursor = cursor->next)
    {
       fprintf (stderr, "[%d] %d", count, cursor->timestamp_status);
       switch (cursor->status)
       {
         case PB_INDEX_UNKNOWN: 
           fprintf (stderr, " PB_INDEX_UNKNOWN"); break;
         case PB_INDEX_SEEN:
           fprintf (stderr, " PB_INDEX_SEEN");  break;
         case PB_INDEX_RECURSIVE: 
           fprintf (stderr, " PB_INDEX_RECURSIVE"); break;
         case PB_INDEX_DONE: 
           fprintf (stderr, " PB_INDEX_DONE"); break;
         default:
           fprintf (stderr, " Invalid status"); break;
       }
       if (cursor->name) 
         fprintf (stderr, " %s\n", cursor->name);
       else
         fprintf (stderr, " Invalid names\n");
       count++;
    }
}
/* Free the indexed header list.  */
void
free_indexed_header_list ()
{
  struct indexed_header *cursor;
  struct indexed_header *next_cursor;
  cursor = next_cursor = indexed_header_list;
  for ( ; next_cursor != NULL; )
    {
      next_cursor = cursor->next;
      free (cursor->name);
      free (cursor);
    }

  return;
}

/* Update header status.  */
void
update_header_status (header, when, found)
     struct indexed_header *header;
     int when;
     int found;
{
  if (header == NULL)
    return;

  if (when == PB_INDEX_BEGIN)
    {
      if (found == 1)
        {
          skip_index_generation++;
          flag_gen_index = 0;
          //fprintf (stderr, "INCREMENT skip count %d:%s ", skip_index_generation, header->name);
        }
      else
        {
          switch (header->status)
            {
              case PB_INDEX_UNKNOWN:
                /* First time this header is encountered. Mark it as seen and
                   generate indexing information for this header.  */
                if (skip_index_generation == 0)
                  {
                    header->status = PB_INDEX_SEEN;
                    flag_gen_index = 1;
                    //fprintf (stderr, "BEGIN Index generation :%s\n", header->name);
                    gen_indexing_info (INDEX_FILE_INCLUDE, header->name, -1);
                    gen_indexing_info (INDEX_FILE_BEGIN, header->name, -1);
                  }
                else
                  {
                    /* Not indexed header inside already indexed header. Ignore!
                       This happens, when one header is included more than once 
                       with different macro settings.  */
                    skip_index_generation++;   
                    header->status = PB_INDEX_DONE;
                    flag_gen_index = 0;
                  }
                break;

              case PB_INDEX_SEEN:
              case PB_INDEX_RECURSIVE:
                /* This header is seen earlier but not yet done, which means recursion.
                   Turn off index generation.  */
                recursion_depth++;
                header->status = PB_INDEX_RECURSIVE;
                flag_gen_index = 0;
                //fprintf (stderr, "RECURSIVE header begin :%s ", header->name);
                break;

              case PB_INDEX_DONE:
                /* This header is already done but not in the list. Something is wrong.  */
                warning("Invalid index header status PB_INDEX_DONE encountered.");
                warning("Indexing information is not generated properly.");
                break;

              default:
                warning("Invalid index header status encountered.");
                warning("Indexing information is not generated properly.");
                break;
            }
        }
    }

  else if (when == PB_INDEX_END)
    {
      if (found == 1)
        {
          skip_index_generation--;
          //fprintf (stderr, "DECREMENT skip count %d:%s ", skip_index_generation, header->name);
        }
      else
        {
          switch (header->status)
            {
              case PB_INDEX_UNKNOWN:
              case PB_INDEX_SEEN:
                /* Finish processing this header and mark accordingly.  */
                header->status = PB_INDEX_DONE;
                //fprintf (stderr, "END index generation :%s\n", header->name);
                gen_indexing_info (INDEX_FILE_END, header->name, -1);
                break;

              case PB_INDEX_RECURSIVE:
                /* Finish processing recursively included header.
                   Decrement recursion_depth count and mark the header as seen.  */
                recursion_depth--;
                header->status = PB_INDEX_SEEN;
                //fprintf (stderr, "RECURSIVE header end :%s ", header->name);
                break;

              case PB_INDEX_DONE:
                /* This header is already done but not in the list. Something is wrong.  */
                warning("Invalid index header status PB_INDEX_DONE encountered.");
                warning("Indexing information is not generated properly.");
                break;

              default:
                warning("Invalid index header status encountered.");
                warning("Indexing information is not generated properly.");
                break;
            }
        }
     if (skip_index_generation == 0 && recursion_depth == 0)
       flag_gen_index = 1;
     else if (skip_index_generation < 0)
       {
         warning("Invalid skip header index count.");
         warning("Indexing information is not generated properly.");
       }
     else if (recursion_depth < 0)
       {
         warning("Invalid recursion depth count.");
         warning("Indexing information is not generated properly.");
       }
    }

  //if (flag_gen_index == 1)
   //fprintf (stderr, " ON\n");
  //else
   //fprintf (stderr, " OFF\n");
}

/* Allocate stack to keep the header begins.  */
static void 
allocate_begin_header_stack ()
{
  begin_header_stack = xmalloc (sizeof (char *) * MAX_BEGIN_COUNT);
}

/* Reallocate stack to accomodate more header names.  */
static void
reallocate_begin_header_stack ()
{
  begin_header_stack = xrealloc (begin_header_stack, MAX_BEGIN_COUNT);
}

/* Pop header name from the begin_header_stack.  */
static char *
pop_begin_header_stack ()
{
  if (begin_header_count < 0)
    {
      warning("Invalid begin_header_count");
      warning("Indexing information is not generated properly.");
      return NULL;
    }
 
  begin_header_count = begin_header_count - 1;

  //fprintf (stderr, "POP:%d:%s\n", begin_header_count, 
  //           begin_header_stack [ begin_header_count]);
  return begin_header_stack [ begin_header_count];
}

/* Push name in the begin_header_stack.  */
static void
push_begin_header_stack (name)
    char *name;
{

  if (begin_header_count == 0)
    allocate_begin_header_stack ();
  else if (begin_header_count >= MAX_BEGIN_COUNT)
    {
       MAX_BEGIN_COUNT = 2 * MAX_BEGIN_COUNT;
       reallocate_begin_header_stack ();
    }

  //fprintf (stderr, "PUSH:%d:%s\n", begin_header_count, name);
  begin_header_stack [ begin_header_count ] = name;
  begin_header_count++;

  return;
}

/* Allocates memory.
   Return absolute path name for the given input filename.  */
static char *
absolute_path_name(char *input_name)
{
  char *name;
  if (input_name[0] != '/') 
    {       
      /* Append current pwd. We need absolute path.  */
      int alen = MAXPATHLEN + strlen (input_name) + 2;
      name = (char *) xmalloc (sizeof (char) * alen);
      name = getcwd(name, alen);
      strcat (name, "/");
      strcat (name, input_name);
    }       
  else
    {       
      name = (char *) xmalloc (strlen (input_name) + 1);
      strcpy (name, input_name);
    }       
  return name;
}

/* Find the name in the indexed header list. 
   Return 1 if found otherwise return 0. */
int
process_header_indexing (input_name, when)
     char *input_name;
     int when;
{
  struct indexed_header *cursor = NULL;
  struct stat buf;
  int found = 0;
  char *name = NULL;

#if 1
  if (flag_cpp_precomp)
    {
      if (when == PB_INDEX_END)
        name = pop_begin_header_stack ();
      else
        {
          name = absolute_path_name (input_name);
          push_begin_header_stack (name);
        }
    }
  else
    name = absolute_path_name (input_name);
#endif
 
  if (!name)
    return 0;

  for (cursor = indexed_header_list; cursor != NULL; cursor = cursor->next)
    {
      if (!strcmp (cursor->name, name))
        {
          if (cursor->timestamp_status == INDEX_TIMESTAMP_VALID)
            {
              if (cursor->status == PB_INDEX_DONE)
                found = 1;
              break;
            }
          else if (cursor->timestamp_status == INDEX_TIMESTAMP_INVALID)
            {
              found = 0;
            }
				  else if (!stat(name, &buf) && buf.st_mtime == cursor->timestamp)
            {
              cursor->timestamp_status = INDEX_TIMESTAMP_VALID;
              if (cursor->status == PB_INDEX_DONE)
                found = 1;
              break;
            }
          else
            {
              cursor->timestamp_status = INDEX_TIMESTAMP_INVALID;
              found = 0;
            }
        }
    }

  if (!found)
    {   
      if (!cursor)
        {   
          cursor = add_index_header_name (name);
          if (cursor)
            {
              cursor->status = PB_INDEX_UNKNOWN;
              cursor->timestamp_status = INDEX_TIMESTAMP_VALID;
            }
        }
    }

  update_header_status (cursor, when, found);
  if (flag_cpp_precomp && when == PB_INDEX_END)
    free (name);
  return found;
}


/* Set indexing language */
void
set_index_lang (int l)
{
  index_language = l;
}
