/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.server.support;

/**
 * <description> 
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public interface TestMBean
{
   public String getThisWillScream() throws MyScreamingException;
   public void setThisWillScream(String str) throws MyScreamingException;
   
   public String getThrowUncheckedException();
   public void setThrowUncheckedException(String str);
   
   public String getError();
   public void setError(String str);
   
   public void setAStringAttribute(String str);
   
   public void operationWithException() throws MyScreamingException;
}
      



