
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.deployment;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.ClassNotFoundException;
import java.util.Collection;


/**
 * IncompleteDeploymentException.java
 *
 *
 * Created: Mon Jun 24 08:20:16 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class IncompleteDeploymentException
   extends DeploymentException
{

   private transient final Collection mbeansWaitingForClasses;
   private transient final Collection mbeansWaitingForDepends;
   private transient final Collection incompletePackages;
   private transient final Collection waitingForDeployer;

   private String string; //on serialization only this should be transferred

   public IncompleteDeploymentException(final Collection mbeansWaitingForClasses, 
                                        final Collection mbeansWaitingForDepends,
                                        final Collection incompletePackages,
                                        final Collection waitingForDeployer) 
   {
      if (mbeansWaitingForClasses == null 
          || mbeansWaitingForDepends == null
          ||incompletePackages == null
          || waitingForDeployer == null) 
      {
         throw new IllegalArgumentException("All lists in IncompleteDeploymentException constructor must be supplied");
      } // end of if ()
      
      this.mbeansWaitingForClasses = mbeansWaitingForClasses;
      this.mbeansWaitingForDepends = mbeansWaitingForDepends;
      this.incompletePackages = incompletePackages;
      this.waitingForDeployer = waitingForDeployer;
   }


   /**
    * Get the MbeansWaitingForClasses value.
    * @return the MbeansWaitingForClasses value.
    */
   public Collection getMbeansWaitingForClasses()
   {
      return mbeansWaitingForClasses;
   }


   /**
    * Get the MbeansWaitingForDepends value.
    * @return the MbeansWaitingForDepends value.
    */
   public Collection getMbeansWaitingForDepends()
   {
      return mbeansWaitingForDepends;
   }

   /**
    * Get the IncompletePackages value.
    * @return the IncompletePackages value.
    */
   public Collection getIncompletePackages()
   {
      return incompletePackages;
   }


   /**
    * Get the WaitingForDeployer value.
    * @return the WaitingForDeployer value.
    */
   public Collection getWaitingForDeployer()
   {
      return waitingForDeployer;
   }

   public boolean isEmpty()
   {
      return mbeansWaitingForClasses.size() == 0 
         && mbeansWaitingForDepends.size() == 0
         && incompletePackages.size() == 0
         && waitingForDeployer.size() == 0;
   }

   public String toString()
   {
      //
      // jason: this is a complete mess... should let the catcher format this
      //
      
      if (string != null) 
      {
         return string;
      } // end of if ()
      
      StringBuffer result = new StringBuffer("Incomplete Deployment listing:\n");
      result.append("Packages waiting for a deployer:\n");
      if (waitingForDeployer.size() == 0) 
      {
         result.append("  <none>\n");
      } // end of if ()
      else
      {
         result.append(waitingForDeployer.toString());
      } // end of else
      
      result.append("Incompletely deployed packages:\n");
      if (incompletePackages.size() == 0) 
      {
         result.append("  <none>\n");
      } // end of if ()
      else
      {
         result.append(incompletePackages.toString());
      } // end of else
      
      result.append("MBeans waiting for classes:\n");
      if (mbeansWaitingForClasses.size() == 0) 
      {
         result.append("  <none>\n");
      } // end of if ()
      else
      {
         result.append(mbeansWaitingForClasses.toString());
      } // end of else
      
   
      result.append("MBeans waiting for other MBeans:\n");
      if (mbeansWaitingForDepends.size() == 0) 
      {
         result.append("  <none>\n");
      } // end of if ()
      else
      {
         result.append(mbeansWaitingForDepends.toString());
      } // end of else
      string = result.toString();
      return string;
   }
      
   private void readObject(ObjectInputStream s) throws IOException, ClassNotFoundException
   {
      s.defaultReadObject();
   }

   private void writeObject(ObjectOutputStream s) throws IOException
   {
      toString();
      s.defaultWriteObject();
   }

}
