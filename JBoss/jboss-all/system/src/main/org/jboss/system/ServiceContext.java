/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.io.Serializable;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import javax.management.ObjectName;

/**
 * ServiceContext holds information for the Service
 *
 * @see Service
 * @see ServiceMBeanSupport
 * 
 * @author <a href="mailto:marc.fleury@jboss.org">marc fleury</a>
 * @version $Revision: 1.4.2.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>20011219 marc fleury:</b>
 * <ul>
 * <li> initial check in.
 * </ul>
 */

public class ServiceContext implements Serializable
{
   
   
   public static int INSTALLED = 0;
   public static int CONFIGURED = 1;
   public static int CREATED = 2;
   public static int RUNNING = 3;
   public static int FAILED = 4;
   public static int STOPPED = 5;
   public static int DESTROYED = 6;
   public static int NOTYETINSTALLED = 7;
   
   private static String[] stateNames = 
   {"INSTALLED",
    "CONFIGURED",
    "CREATED",
    "RUNNING",
    "FAILED",
    "STOPPED",
    "DESTROYED",
    "NOTYETINSTALLED"};

   /** The name of the service **/
   public ObjectName objectName;
   
   /** State of the service **/
   public int state = NOTYETINSTALLED;
   
   /** dependent beans **/
   public List iDependOn = new LinkedList();
   
   /** beans that depend on me **/
   public List dependsOnMe = new LinkedList();
   
   /** the fancy proxy to my service calls **/
   public Service proxy;

   public Throwable problem;

   public String toString()
   {
      return "ObjectName: " + objectName + "\n state: " + stateNames[state] + "\n I Depend On: " + printList(iDependOn) + "\n Depends On Me: " + printList(dependsOnMe) + printProblem();
   }
   
   private String printList(List ctxs)
   {
      StringBuffer result = new StringBuffer();
      for (Iterator i = ctxs.iterator(); i.hasNext();)
      {
         ServiceContext sc = (ServiceContext) i.next();
         result.append(' ');
         result.append(sc.objectName);
         result.append('\n');
      } // end of for ()
      return result.toString();
   }
      
   private String printProblem()
   {
      if (state == FAILED && problem != null) 
      {
         return problem.toString();
      } // end of if ()
      return "";
   }

}
