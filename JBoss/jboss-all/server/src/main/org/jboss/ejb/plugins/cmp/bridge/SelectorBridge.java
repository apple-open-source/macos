/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.bridge;

import java.lang.reflect.Method;
import javax.ejb.FinderException;

/**
 * SelectorBridge represents one ejbSelect method. 
 *
 * Life-cycle:
 *      Tied to the EntityBridge.
 *
 * Multiplicity:   
 *      One for each entity bean ejbSelect method.       
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.5 $
 */                            
public interface SelectorBridge {
   public String getSelectorName();
   public Method getMethod();
      
   public Object execute(Object[] args) throws FinderException;
}
