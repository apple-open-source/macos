/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.testbeancluster.bean;

import javax.ejb.*;

import java.rmi.RemoteException;
import java.rmi.dgc.VMID;
import org.jboss.test.testbeancluster.interfaces.NodeAnswer;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>6. octobre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class StatefulSessionBean extends org.jboss.test.testbean.bean.StatefulSessionBean 
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   public transient VMID myId = null; 
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
  public void ejbCreate(String name) throws RemoteException, CreateException 
  {
     super.ejbCreate(name);
     
     this.myId = new VMID ();
     log.debug ("My ID: " + this.myId);
  }

  public void ejbActivate() throws RemoteException {
      super.ejbActivate();
      if (this.myId == null)
      {
         //it is a failover: we need to assign ourself an id
         this.myId = new VMID ();
      }
      log.debug("Activate. My ID: " + this.myId + " name: " + this.name);
  }

  public void ejbPassivate() throws RemoteException {
      super.ejbPassivate();
     log.debug("Passivate. My ID: " + this.myId + " name: " + this.name);
  }
   // Public --------------------------------------------------------
   
   // Remote Interface implementation ----------------------------------------------
   
   public NodeAnswer getNodeState () throws RemoteException
   {
      return new NodeAnswer (this.myId, this.name);
   }

   public void setName (String name) throws RemoteException
   {
      this.name = name;
      log.debug ("Name set to " + name);
   }
   
   public void setNameOnlyOnNode (String name, VMID node) throws RemoteException
   {
      if (node.equals (this.myId))
         this.setName (name);
      else
         throw new EJBException ("Trying to assign value on node " + this.myId + " but this node expected: " + node);
   }

   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
