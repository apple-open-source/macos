/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import org.jboss.system.Service;

/**
 * This is a superinterface for all Container plugins.
 * 
 * <p>All plugin interfaces must extend this interface.
 *      
 * @see Service
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @version $Revision: 1.9 $
 */
public interface ContainerPlugin
   extends Service
{
   /**
    * This callback is set by the container so that the plugin may access it
    *
    * @param con The container using this plugin. This may be null if the
    plugin is being disassociated from a container.
    */
   void setContainer(Container con);
}
