/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy.ejb;

import java.util.Map;

/**
 * This interface is used for EJB CMP 2.0 read ahead algorithm for holding read ahead values.
 *
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @version $Revision: 1.1 $
 */
public interface ReadAheadBuffer
{

   /**
    * The map of read ahead values, maps Methods to values.
    */
   public Map getReadAheadValues();
}


