/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.threading.mbean;


/**
*   <description> 
*
*   @see <related>
*   @author  <a href="mailto:marc@jboss.org">Marc Fleury</a>
*   @version $Revision: 1.1 $
*   
*   Revisions:
*
*   20010625 marc fleury: Initial version
*/

public interface ThreadsMBean
{

	public void setNumberOfThreads(int numberOfThreads) ;
   public int getNumberOfThreads();
	public void setWait(long numberOfThreads) ;
   public long getWait();
	
	public void startMe() throws Exception;
	
	public void stopMe();
}
