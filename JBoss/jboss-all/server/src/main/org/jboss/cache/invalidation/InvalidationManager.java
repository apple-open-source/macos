/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import java.io.Serializable;
import java.util.HashSet;

import javax.management.MBeanParameterInfo;
import javax.management.MBeanOperationInfo;

import org.jboss.cache.invalidation.InvalidationManager.BridgeInvalidationSubscriptionImpl;

/**
 * Implementation of InvalidationManagerMBean
 *
 * @see org.jboss.cache.invalidation.InvalidationManagerMBean
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>21 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class InvalidationManager 
   extends org.jboss.system.ServiceMBeanSupport 
   implements InvalidationManagerMBean 
{
   
   // Constants -----------------------------------------------------
   
   public static final String DEFAULT_JMX_SERVICE_NAME = "jboss.cache:service=InvalidationManager";
   public static final String DEFAULT_INVALIDERS_JMX_NAME = "jboss.cache:service=InvalidationGroup";
   
   // Attributes ----------------------------------------------------
   
   protected java.util.Hashtable groups = new java.util.Hashtable ();
   protected java.util.Vector bridgeSubscribers = new java.util.Vector ();
   protected int hashcode = 0;
   
   protected boolean DEFAULT_TO_ASYNCHRONOUS_MODE = false;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public InvalidationManager () { super(); }
   
   public void startService () throws Exception
   {
      // bind us in system registry
      //
      log.info ("Starting Invalidation Manager " + this.getServiceName ().toString ());
      org.jboss.system.Registry.bind (this.getServiceName ().toString (), this);
      this.hashcode = this.getServiceName ().hashCode ();
   }
   
   public void stopService ()
   {
      log.info ("Stoping Invalidation Manager " + this.getServiceName ().toString ());
      org.jboss.system.Registry.unbind (this.getServiceName ().toString ());
   }
   
   
   public java.util.Collection getInvalidationGroups ()
   {
      return this.groups.values ();
   }
   
   public InvalidationGroup getInvalidationGroup (String groupName)
   {
      synchronized (this.groups)
      {
         InvalidationGroup group = (InvalidationGroup)this.groups.get (groupName);
         if (group == null)
         {
            group = createGroup (groupName);
         }

         group.addReference ();

         return group;
      }
   }   

   public synchronized BridgeInvalidationSubscription registerBridgeListener (InvalidationBridgeListener listener)
   {
      log.info ("Subscribing a new cache-invalidation bridge");
      BridgeInvalidationSubscription subs = new BridgeInvalidationSubscriptionImpl(listener);

      java.util.Vector newVector = new java.util.Vector (this.bridgeSubscribers);
      newVector.add (subs);
      this.bridgeSubscribers = newVector;      
      
      return subs;
   }
   
   public void batchInvalidate (BatchInvalidation[] invalidations)
   {
      this.batchInvalidate (invalidations, this.DEFAULT_TO_ASYNCHRONOUS_MODE);
   }
   
   public void batchInvalidate (BatchInvalidation[] invalidations, boolean asynchronous)
   {
      if (log.isTraceEnabled ())
         log.trace ("Batch cache invalidation. Caches concerned: " + invalidations.length);
      
      this.crossDomainBatchInvalidate (null, invalidations, this.DEFAULT_TO_ASYNCHRONOUS_MODE);      
   }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Object overrides ---------------------------------------------------
   
   public int hashCode ()
   {
      return hashcode;
   }   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------

   protected InvalidationGroup createGroup (String groupName)
   {
      InvalidationGroup group = new org.jboss.cache.invalidation.InvalidationManager.InvalidationGroupImpl (groupName);
      this.groups.put (groupName, group);

      // register the group with JMX so it can be easyly remotly
      // reached (and thus cache can be invalidated)
      //
      try
      {
         log.debug ("Creating and registering a new InvalidationGroup: " + groupName);
         javax.management.ObjectName groupObjectName = new javax.management.ObjectName (DEFAULT_INVALIDERS_JMX_NAME + ",GroupName="+groupName);
         this.getServer ().registerMBean (group, groupObjectName);      
      }
      catch (Exception e)
      {
         log.info ("Problem while trying to register a new invalidation group in JMX", e);
      }

      // warn bridges
      //
      log.debug ("Informing bridges about new group creation ...");
      for (int i=0; i<bridgeSubscribers.size (); i++)
         ((BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i))).groupCreated (groupName);
      
      return group;
   }
   
   protected void removeGroup (String groupName)
   {
      synchronized (this.groups)
      {
         this.groups.remove (groupName);

         // Remove group from JMX
         //
         try
         {
            log.debug ("Removing and JMX-unregistering an InvalidationGroup: " + groupName);
            javax.management.ObjectName groupObjectName = new javax.management.ObjectName (DEFAULT_INVALIDERS_JMX_NAME + ",GroupName="+groupName);
            this.getServer ().unregisterMBean (groupObjectName);      
         }
         catch (Exception e)
         {
            log.info ("Problem while trying to un-register a new invalidation group in JMX", e);
         }

         // warn bridges
         //
         for (int i=0; i<bridgeSubscribers.size (); i++)
            ((BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i))).groupDropped (groupName);
      }
               
   }
   
   protected synchronized void unregisterBridgeListener (BridgeInvalidationSubscription bridgeSubscriber)
   {
      // safe remove to avoid problems with iterators
      //
      log.info ("Unsubscription of a cache-invalidation bridge");
      
      java.util.Vector newVector = new java.util.Vector (this.bridgeSubscribers);
      newVector.remove (bridgeSubscriber);
      this.bridgeSubscribers = newVector;      
   }
   
   protected void doLocalOnlyInvalidation (String groupName, Serializable key, boolean asynchronous)
   {
      InvalidationGroupImpl group = (InvalidationGroupImpl)this.groups.get (groupName);
      if (group != null)
         group.localOnlyInvalidate (key, asynchronous);      
   }
   
   protected void doLocalOnlyInvalidations (String groupName, Serializable[] keys, boolean asynchronous)
   {
      InvalidationGroupImpl group = (InvalidationGroupImpl)this.groups.get (groupName);
      if (group != null)
         group.localOnlyInvalidate (keys, asynchronous);      
   }
   
   protected void doBridgedOnlyInvalidation (BridgeInvalidationSubscriptionImpl exceptSource, String groupName, Serializable key)
   {
      for (int i=0; i<bridgeSubscribers.size (); i++)
      {
         BridgeInvalidationSubscriptionImpl bridge = (BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i));
         if (bridge != exceptSource)
            bridge.bridgedInvalidate (groupName, key, this.DEFAULT_TO_ASYNCHRONOUS_MODE);
      }
   }
   
   protected void doBridgedOnlyInvalidation (BridgeInvalidationSubscriptionImpl exceptSource, String groupName, Serializable[] keys)
   {
      for (int i=0; i<bridgeSubscribers.size (); i++)
      {
         BridgeInvalidationSubscriptionImpl bridge = (BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i));
         if (bridge != exceptSource)
            bridge.bridgedInvalidate (groupName, keys, this.DEFAULT_TO_ASYNCHRONOUS_MODE);
      }
   }
   // this is called when an invalidation occurs in one of the group. Common behaviour
   // can be groupped here. By default, we simply forward the invalidations to the
   // available bridges.
   //
   protected void localGroupInvalidationEvent (String groupName, Serializable key, boolean asynchronous)
   {
      for (int i=0; i<bridgeSubscribers.size (); i++)
         ((BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i))).bridgedInvalidate (groupName, key, asynchronous);      
   }
   
   protected void localGroupInvalidationsEvent (String groupName, Serializable[] keys, boolean asynchronous)
   {
      for (int i=0; i<bridgeSubscribers.size (); i++)
         ((BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i))).bridgedInvalidate (groupName, keys, asynchronous);      
   }


   // We warn other groups and the local group (if available)
   //
   protected void bridgeGroupInvalidationEvent (BridgeInvalidationSubscriptionImpl source, String groupName, Serializable key)
   {
      doBridgedOnlyInvalidation (source, groupName, key);      
      doLocalOnlyInvalidation (groupName, key, this.DEFAULT_TO_ASYNCHRONOUS_MODE);
   }
   
   protected void bridgeGroupInvalidationEvent (BridgeInvalidationSubscriptionImpl source, String groupName, Serializable[] keys)
   {
      doBridgedOnlyInvalidation (source, groupName, keys);      
      doLocalOnlyInvalidation (groupName, keys, this.DEFAULT_TO_ASYNCHRONOUS_MODE);
   }
   
   protected void crossDomainBatchInvalidate (BridgeInvalidationSubscriptionImpl source, BatchInvalidation[] invalidations, boolean asynchronous)
   {
      if (invalidations == null)
         return;
      
      // local invalidation first
      //
      for (int i=0; i<invalidations.length; i++)
      {
         BatchInvalidation currInvalid = invalidations[i];
         
         doLocalOnlyInvalidations (currInvalid.getInvalidationGroupName (),
                                  currInvalid.getIds (), 
                                  asynchronous);
      }
      
      // bridged invalidation next
      //
      for (int i=0; i<bridgeSubscribers.size (); i++)
      {
         BridgeInvalidationSubscriptionImpl bridge = (BridgeInvalidationSubscriptionImpl)(bridgeSubscribers.elementAt (i));
         if (bridge != source)
            bridge.bridgedBatchInvalidations  (invalidations, asynchronous);
      }
      
      
   }
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
   /**
    * This class implements the InvalidationGroup interface. It represent the 
    * meeting point of caches and invaliders for a same group.
    */
   class InvalidationGroupImpl implements InvalidationGroup, javax.management.DynamicMBean
   {
      protected org.jboss.logging.Logger igLog = null;      
      protected String groupName = null;
      protected boolean asynchronous = DEFAULT_TO_ASYNCHRONOUS_MODE;
      protected HashSet registered = new HashSet();
      protected int counter = 0;
      
      public int hashCode ()
      {
         return groupName.hashCode ();
      }
      
      public String getGroupName ()
      {
         return this.groupName;
      }
      
      public InvalidationGroupImpl (String groupName)
      {
         this.groupName = groupName;
         this.igLog = org.jboss.logging.Logger.getLogger(getClass() + "." + groupName);
      }
      
      public InvalidationManagerMBean getInvalidationManager ()
      {
         return InvalidationManager.this;
      }

      public void invalidate (Serializable key)
      {
         this.invalidate (key, this.asynchronous);
      }
      
      public void invalidate (Serializable key, boolean asynchronous)
      {
         localOnlyInvalidate (key, asynchronous);
         
         localGroupInvalidationEvent (this.groupName, key, asynchronous);
      }

      public void invalidate (Serializable[] keys)
      {
         this.invalidate (keys, this.asynchronous);
      }
      
      public void invalidate (Serializable[] keys, boolean asynchronous)
      {
         localOnlyInvalidate (keys, asynchronous);
         
         localGroupInvalidationsEvent (this.groupName, keys, asynchronous);
      }
      
      public synchronized void register (Invalidatable newRegistered)
      {
         // we make a temp copy to avoid concurrency issues with the invalidate method
         //
         HashSet newlyRegistered = new HashSet (this.registered);         
         newlyRegistered.add (newRegistered);
         
         this.registered = newlyRegistered;                  
      }
      
      public synchronized void unregister (Invalidatable oldRegistered)
      {
         // we make a temp copy to avoid concurrency issues with the invalidate method
         //
         HashSet newlyRegistered = new HashSet(this.registered);         
         newlyRegistered.remove (oldRegistered);
         
         this.registered = newlyRegistered;
         
         this.removeReference ();
      }

      public void setAsynchronousInvalidation (boolean async)
      {
         this.asynchronous = async;
      }
      
      public boolean getAsynchronousInvalidation ()
      {
         return this.asynchronous;
      }
   
      public void addReference ()
      {         
         counter++;
         igLog.debug ("Counter reference value (++): " + counter);
      }
      
      public int getReferenceCount ()
      {
         return this.counter;
      }
      
      public void removeReference ()
      {
         counter--;
         igLog.debug ("Counter reference value (--): " + counter);
         
         if (counter<=0)
         {
            removeGroup (this.groupName);
                                 
            //Iterator iter = this.registered.iterator ();
            //while (iter.hasNext ())
            //   ((Invalidatable)iter.next ()).groupIsDropped () ;
         }
      }
      
      // DynamicMBean implementation ----------------------------------------------
      
      public Object getAttribute (String attribute) throws javax.management.AttributeNotFoundException, javax.management.MBeanException, javax.management.ReflectionException
      {
         if (attribute == null || attribute.equals (""))
            throw new IllegalArgumentException ("null or empty attribute name");
         
         if (attribute.equals ("AsynchronousInvalidation"))
            return new Boolean(this.asynchronous);
         else
            throw new javax.management.AttributeNotFoundException(attribute + " is not a known attribute");
      }
      
      public javax.management.AttributeList getAttributes (java.lang.String[] attributes)
      {
         return null;
      }
      
      public javax.management.MBeanInfo getMBeanInfo ()
      {
         
         MBeanParameterInfo serSimpleParam = new MBeanParameterInfo (
                  "key", 
                  Serializable.class.getName (), 
                  "Primary key to be invalidated"
         );
      
         MBeanParameterInfo serArrayParam = new MBeanParameterInfo (
                  "keys", 
                  Serializable[].class .getName (), 
                  "Primary keys to be invalidated"
         );
      
         MBeanParameterInfo asynchParam = new MBeanParameterInfo (
                  "asynchronous", 
                  Boolean.class.getName (), 
                  "Indicates if the invalidation should be asynchronous or must be synchronous"
         );
      
      javax.management.MBeanAttributeInfo[] attrInfo = new javax.management.MBeanAttributeInfo[] {
       new javax.management.MBeanAttributeInfo("AsynchronousInvalidation",
               Boolean.class.getName(),
               "Indicates if invalidation, by default, should be done asynchronously",
               true,
               true,
               false)};

         MBeanOperationInfo[] opInfo = {
            new MBeanOperationInfo("invalidate",
                                   "invalidate a single key using default (a)synchronous behaviour",
                                   new MBeanParameterInfo[] {serSimpleParam},
                                   void.class.getName(),
                                   MBeanOperationInfo.ACTION),

            new MBeanOperationInfo("invalidate",
                                   "invalidate a single key indicating the (a)synchronous behaviour",
                                   new MBeanParameterInfo[] {serSimpleParam, asynchParam},
                                   void.class.getName(),
                                   MBeanOperationInfo.ACTION),

            new MBeanOperationInfo("invalidate",
                                   "invalidate multiple keys using default (a)synchronous behaviour",
                                   new MBeanParameterInfo[] {serArrayParam},
                                   void.class.getName(),
                                   MBeanOperationInfo.ACTION),

            new MBeanOperationInfo("invalidate",
                                   "invalidate multiple keys indicating the (a)synchronous behaviour",
                                   new MBeanParameterInfo[] {serArrayParam, asynchParam},
                                   void.class.getName(),
                                   MBeanOperationInfo.ACTION),

         };

         javax.management.MBeanNotificationInfo[] notifyInfo = null;
         javax.management.MBeanConstructorInfo[] ctorInfo = new  javax.management.MBeanConstructorInfo[] {};

         return new javax.management.MBeanInfo(getClass().getName(),
                              "Cache invalidation for group named " + this.groupName,
                              attrInfo,
                              ctorInfo,
                              opInfo,
                              notifyInfo);
      }
      
      public java.lang.Object invoke (java.lang.String actionName, java.lang.Object[] params, java.lang.String[] signature) throws javax.management.MBeanException, javax.management.ReflectionException
      {
         if (!"invalidate".equals (actionName))
            throw new IllegalArgumentException ("Unknown operation: " + actionName);
         else
         {
            if (params.length == 1)
            {
               if (params[0] instanceof Serializable[])                  
                  this.invalidate ((Serializable[])params[0]);
               else if (params[0] instanceof Serializable)
                  this.invalidate ((Serializable)params[0]);
               else
                  throw new IllegalArgumentException ("First argument must be Serializable (or array of)");                  
            }
            else if (params.length == 2)
            {
               if (params[0] instanceof Serializable[])                  
                  this.invalidate ((Serializable[])params[0], ((Boolean)params[1]).booleanValue ());
               else if (params[0] instanceof Serializable)
                  this.invalidate ((Serializable)params[0], ((Boolean)params[1]).booleanValue ());
               else
                  throw new IllegalArgumentException ("First argument must be Serializable (or array of)");
            }
            else
            {
               throw new IllegalArgumentException ("Unknown operation with these parameters: " + actionName);
            }
         }
         return null;
      }      
      
      public void setAttribute (javax.management.Attribute attribute) throws javax.management.AttributeNotFoundException, javax.management.InvalidAttributeValueException, javax.management.MBeanException, javax.management.ReflectionException
      {
         String attrName = attribute.getName();
        if (attrName == null || attrName.equals (""))
            throw new IllegalArgumentException ("null or empty attribute name");
         
         if (attrName.equals ("AsynchronousInvalidation"))
         {
            Object value = attribute.getValue ();
            if (value instanceof Boolean)
               this.asynchronous = ((Boolean)value).booleanValue ();
            else
               throw new javax.management.InvalidAttributeValueException("Attribute is of boolean type");
         }
         else
            throw new javax.management.AttributeNotFoundException(attrName + " is not a known attribute");
      }
      
      public javax.management.AttributeList setAttributes (javax.management.AttributeList attributes)
      {
         return null;
      }
      
      // Protected ------------------------------------------------------------------------
      
      protected void localOnlyInvalidate (Serializable[] keys, boolean asynchronous)
      {
         java.util.Iterator iter = this.registered.iterator ();
         while (iter.hasNext ())
         {
            Invalidatable inv = (Invalidatable)iter.next ();
            inv.areInvalid (keys);
         }
      }
      
      protected void localOnlyInvalidate (Serializable key, boolean asynchronous)
      {              
         java.util.Iterator iter = this.registered.iterator ();
         while (iter.hasNext ())
         {
            Invalidatable inv = (Invalidatable)iter.next ();
            inv.isInvalid (key);
         }

      }

   }
   
   // *******************************************************************************************33
   // *******************************************************************************************33
   // *******************************************************************************************33
   
   class BridgeInvalidationSubscriptionImpl 
      implements BridgeInvalidationSubscription
   {
      
      protected InvalidationBridgeListener listener = null;
      
      public BridgeInvalidationSubscriptionImpl (InvalidationBridgeListener listener)
      {
         this.listener = listener;
      }
      
      public void invalidate (String invalidationGroupName, Serializable key)
      {
         bridgeGroupInvalidationEvent (this, invalidationGroupName, key);
      }
      
      public void invalidate (String invalidationGroupName, Serializable[] keys)
      {
         bridgeGroupInvalidationEvent (this, invalidationGroupName, keys);
      }
      
      public void batchInvalidate (BatchInvalidation[] invalidations)
      {
         crossDomainBatchInvalidate (this, invalidations, DEFAULT_TO_ASYNCHRONOUS_MODE);
         
      }
      
      public void unregister ()
      {
         unregisterBridgeListener (this);
      }
      
      // Internal callbacks
      //
      
      protected void bridgedInvalidate (String invalidationGroupName, Serializable key, boolean asynchronous)
      {
         this.listener.invalidate (invalidationGroupName, key, asynchronous);
      }
      
      protected void bridgedInvalidate (String invalidationGroupName, Serializable[] keys, boolean asynchronous)
      {
         this.listener.invalidate (invalidationGroupName, keys, asynchronous);
      }
      
      protected void bridgedBatchInvalidations (BatchInvalidation[] invalidations, boolean asynchronous)
      {
         this.listener.batchInvalidate (invalidations, asynchronous);
      }
      
      protected void groupCreated (String invalidationGroupName)
      {
         this.listener.newGroupCreated (invalidationGroupName);
      }

      protected void groupDropped (String invalidationGroupName)
      {
         this.listener.groupIsDropped (invalidationGroupName);
      }
      
   }
}
