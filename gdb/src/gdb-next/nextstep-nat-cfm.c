#include <mach/mach.h>

/* this is to placate bfd */
#define TRUE_FALSE_ALREADY_DEFINED

#include "defs.h"
#include "breakpoint.h"
#include "gdbcmd.h"

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-util.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-cfm.h"
#include "nextstep-nat-cfm-io.h"
#include "nextstep-nat-cfm-process.h"

CFMClosure *gClosures = NULL;

extern next_inferior_status *next_status;

extern OSStatus CCFM_SetInfoSPITarget
(MPProcessID targetProcess, void *targetCFMHook, mach_port_t notifyPort);

static int cfm_annotation_level = 2;

static void
output_data (unsigned char *buf, size_t len)
{
  size_t i;

  printf_filtered ("0x%lx\n", len);
  for (i = 0; i < len; i++) {
    if ((i % 8) != 0) {
      printf_filtered (" ");
    }
    printf_filtered ("0x%02x", buf[i]);
    if (((i + 1) % 8) == 0) {
      printf_filtered ("\n");
    }
  }
  if (((i + 1) % 8) != 0) {
    printf_filtered ("\n");
  }
}

static void
annotate_closure_begin (CFragClosureInfo *closure)
{
  if ((annotation_level > 1) && (cfm_annotation_level > 1))
    {
      printf_filtered ("\n\032\032closure-begin\n");
      output_data ((unsigned char *) closure, sizeof (*closure));
    }
}

static void
annotate_closure_end (CFragClosureInfo *closure)
{
  if ((annotation_level > 1) && (cfm_annotation_level > 1))
    {
      printf_filtered ("\n\032\032closure-end\n");
    }
}

static void
annotate_connection (CFragConnectionInfo *connection)
{
  if ((annotation_level > 1) && (cfm_annotation_level > 1))
    {
      printf_filtered ("\n\032\032connection\n");
      output_data ((unsigned char *) connection, sizeof (*connection));
    }
}

static void
annotate_container (CFragContainerInfo *container)
{
  if ((annotation_level > 1) && (cfm_annotation_level > 1))
    {
      printf_filtered ("\n\032\032container\n");
      output_data ((unsigned char *) container, sizeof (*container));
    }
}

static void
annotate_section (ItemCount index, CFragSectionInfo *section)
{
  if ((annotation_level > 1) && (cfm_annotation_level > 1))
    {
      printf_filtered ("\n\032\032section\n");
      output_data ((unsigned char *) section, sizeof (*section));
    }
}

void
next_init_cfm_info_api (struct next_inferior_status *status)
{
  kern_return_t	kret;
  
  if (status->cfm_status.cfm_receive_right != MACH_PORT_NULL)
    return;

  if (status->cfm_status.info_api_cookie == NULL)
    return;

  if (status->task == MACH_PORT_NULL)
    return;

  next_cfm_thread_create (&status->cfm_status, status->task);
}

void
next_handle_cfm_event (struct next_inferior_status* status, unsigned char *buf)
{
    CFragNotifyInfo *messageBody = (CFragNotifyInfo *) buf;
    kern_return_t result, kret;
    ItemCount numberOfConnections, totalCount, index;
    CFragConnectionID *connections;
    CFragClosureInfo closureInfo;
    CFMClosure *closure;
    
    gdb_flush (gdb_stdout);
    
    kret = next_inferior_suspend_mach (next_status);
    MACH_CHECK_ERROR (kret);

    kret = task_resume (next_status->task);
    MACH_CHECK_ERROR (kret);

    CHECK_FATAL ((kCFragPrepareClosureNotify == messageBody->notifyKind)
		 || (kCFragReleaseClosureNotify == messageBody->notifyKind));

    result = CFragGetClosureInfo
      (messageBody->u.closureInfo.closureID, kCFragClosureInfoVersion, &closureInfo);
    CHECK_FATAL (result == noErr);
    annotate_closure_begin (&closureInfo);

    closure = xcalloc (1, sizeof (CFMClosure));
    closure->mClosure = closureInfo;

    result = CFragGetConnectionsInClosure
      (closureInfo.closureID, 0, 0, &numberOfConnections, NULL);
    CHECK_FATAL (result == noErr);

    connections = xmalloc(numberOfConnections * sizeof(CFragConnectionID));

    result = CFragGetConnectionsInClosure
      (closureInfo.closureID, numberOfConnections, 0, &totalCount, connections);
    CHECK_FATAL (result == noErr);
    CHECK_FATAL (numberOfConnections == totalCount);

    for (index = 0; index < numberOfConnections; ++index)
    {
        CFragConnectionInfo	connectionInfo;
        CFragContainerInfo	containerInfo;
        ItemCount			sectionIndex;
        CFMConnection*		connection;
        CFMContainer*		container;
        
        result = CFragGetConnectionInfo(connections[index],
                                        kCFragConnectionInfoVersion,
                                        &connectionInfo);
        CHECK_FATAL (noErr == result);
        annotate_connection(&connectionInfo);

        connection = xcalloc(1, sizeof(CFMConnection));
        CHECK_FATAL (NULL != connection);
        connection->mConnection = connectionInfo;
        connection->mNext = closure->mConnections;
        closure->mConnections = connection;

        result = CFragGetContainerInfo(connectionInfo.containerID,
                                       kCFragContainerInfoVersion,
                                       &containerInfo);
        CHECK_FATAL (noErr == result);

        annotate_container(&containerInfo);

        container = xcalloc(1, sizeof(CFMContainer));
        CHECK_FATAL (NULL != container);
        container->mContainer = containerInfo;
        connection->mContainer = container;

        for (sectionIndex = 0; sectionIndex < containerInfo.sectionCount; ++sectionIndex)
        {
            CFragSectionInfo	sectionInfo;
            CFMSection*			section;
            
            result = CFragGetSectionInfo(connections[index],
                                         sectionIndex,
                                         kCFragSectionInfoVersion,
                                         &sectionInfo);
            CHECK_FATAL (noErr == result);
            annotate_section(sectionIndex, &sectionInfo);

            section = xcalloc(1, sizeof(CFMSection));
            CHECK_FATAL (NULL != section);
            section->mSection = sectionInfo;
            section->mContainer = container;
            section->mNext = container->mSections;
            container->mSections = section;
        }
    }

    xfree (connections);

    annotate_closure_end(&closureInfo);

    closure->mNext = gClosures;
    gClosures = closure;
       
    next_inferior_suspend_mach (status);

    next_update_cfm ();

    reread_symbols ();
    breakpoint_re_set ();
    breakpoint_update ();
    re_enable_breakpoints_in_shlibs (0);
    enable_breakpoints_in_containers ();
}

void enable_breakpoints_in_containers (void)
{
  struct breakpoint *b;
  extern struct breakpoint* breakpoint_chain;

  for (b = breakpoint_chain; b; b = b->next) {

    if (b->enable != disabled) { break; } 
    if (strncmp (b->addr_string, "@metrowerks:", strlen ("@metrowerks:")) != 0) { break; } 

    {
      char **argv;
      unsigned long section;
      unsigned long offset;
      CFMContainer *cfmContainer;

      argv = buildargv (b->addr_string + strlen ("@metrowerks:"));
      if (argv == NULL) { 
	nomem (0); 
      } 
      
      section = strtoul (argv[1], NULL, 16); 
      offset = strtoul (argv[2], NULL, 16); 

      cfmContainer = CFM_FindContainerByName (argv[0], strlen (argv[0]));
      if (cfmContainer != NULL) {
	CFMSection *cfmSection = CFM_FindSection(cfmContainer, kCFContNormalCode);
	if (cfmSection != NULL) {
	  b->address = cfmSection->mSection.address + offset;
	  b->enable = enabled;
	  b->addr_string = xmalloc (64);
	  sprintf (b->addr_string, "*0x%lx", (unsigned long) b->address);
	}
      }
    }
  }
}

CFMContainer *CFM_FindContainerByName (char *name, int length)
{
    CFMContainer*	found = NULL;
    CFMClosure* 	closure = gClosures;
    
    while ((NULL == found) && (NULL != closure))
    {
        CFMConnection* connection = closure->mConnections;
        while ((NULL == found) && (NULL != connection))
        {
            if (NULL != connection->mContainer)
            {
                CFMContainer* container = connection->mContainer;
                if (0 == memcmp(name,
                                &container->mContainer.name[1],
                                min(container->mContainer.name[0], length)))
                {
                    found = container;
                    break;
                }
            }

            connection = connection->mNext;
        }

        closure = closure->mNext;
    }

    return found;
}

CFMSection *CFM_FindSection (CFMContainer *container, CFContMemoryAccess accessType)
{
  int done = false;
  CFMSection *section = container->mSections;

  while (!done && (NULL != section))
    {
      CFContMemoryAccess access = section->mSection.access;
      if ((access & accessType) == accessType)
        {
	  done = true;
	  break;
        }
        
      section = section->mNext;
    }

  return (done ? section : NULL);
}

CFMSection *CFM_FindContainingSection (CORE_ADDR address)
{
  CFMSection*	section = NULL;
  CFMClosure*	closure = gClosures;

  while ((NULL == section) && (NULL != closure))
    {
      CFMConnection* connection = closure->mConnections;
      while ((NULL == section) && (NULL != connection))
        {
	  CFMContainer*	container = connection->mContainer;
	  CFMSection*		testSection = container->mSections;
	  while ((NULL == section) && (NULL != testSection))
            {
	      if ((address >= testSection->mSection.address) &&
		  (address < (testSection->mSection.address + testSection->mSection.length)))
                {
		  section = testSection;
		  break;
                }
                
	      testSection = testSection->mNext;
            }
            
	  connection = connection->mNext;
        }

      closure = closure->mNext;
    }

  return section;
}

static void info_cfm_command (args, from_tty)
     char *args;
     int from_tty;
{
  CFMClosure *closure = gClosures;
  while (closure != NULL) {

    CFMConnection *connection = closure->mConnections;
    annotate_closure_begin (&closure->mClosure);

    while (connection != NULL) {
      
      CFMContainer *container = connection->mContainer;
      CFMSection *section = container->mSections;
      size_t secnum = 0;

      annotate_container (&container->mContainer);

      printf_filtered 
	("Noted CFM object \"%.*s\" at 0x%lx for 0x%lx\n",
	 container->mContainer.name[0], &container->mContainer.name[1],
	 (unsigned long) container->mContainer.address,
	 (unsigned long) container->mContainer.length);
      
      annotate_connection (&connection->mConnection);

      while (section != NULL) {
	annotate_section (secnum, &section->mSection);
	secnum++;
	section = section->mNext;
      }
      
      connection = connection->mNext;
    }

    annotate_closure_end (&closure->mClosure);

    closure = closure->mNext;
  }

  return;
}

void
_initialize_nextstep_nat_cfm ()
{
  struct cmd_list_element *cmd = NULL;

  cmd = add_set_cmd
    ("cfm-annotate", class_obscure, var_zinteger,
     (char *) &cfm_annotation_level,
     "Set annotation level for CFM structure data.",
     &setlist);
  add_show_from_set (cmd, &showlist);		

  add_info ("cfm", info_cfm_command,
	    "Show current CFM state.");
}
