/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

/**
 * DuplicateServiceException
 *
 * <p>Exception indicating that the add of a duplicate service was attempted.
 * (i.e. the service being added already exists in the config store, for the
 * given scope).
 *
 * @version $Revision: 1.1 $
 * @author  <a href="mailto:bitpushr@rochester.rr.com">Mike Finn</a>.
 *
 */
public class DuplicateServiceException 
   extends java.lang.Exception 
{

    /**
     * Constructs an instance of <code>DuplicateServiceException</code> with the specified detail message.
     * @param msg the detail message.
     */
    public DuplicateServiceException(String msg) 
    {
        super(msg);
    }
}


