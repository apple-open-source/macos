package org.jboss.test.hello.interfaces;

import javax.ejb.EJBLocalObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface LocalHelloLog extends EJBLocalObject
{
   public String getHelloArg();
   public void setHelloArg(String arg);

   long getStartTime();
   void setStartTime(long startTime);

   long getEndTime();
   void setEndTime(long endTime);
}
