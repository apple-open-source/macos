/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.persistence;

import javax.management.MBeanInfo;
import org.jboss.mx.modelmbean.ModelMBeanInvoker;

/**
 * Provides an empty implementation of the <tt>PersistenceManager</tt>
 * interface.
 *
 * @see org.jboss.mx.persistence.PersistenceManager
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author Matt Munz
 * @version $Revision: 1.1.8.1 $
 *   
 */
public class NullPersistence 
   implements PersistenceManager
{

   public void load(ModelMBeanInvoker mbean, MBeanInfo info) {}
   public void store(MBeanInfo info) {}

}
