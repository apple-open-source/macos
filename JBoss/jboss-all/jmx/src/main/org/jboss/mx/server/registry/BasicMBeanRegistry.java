/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.server.registry;

import javax.management.DynamicMBean;
import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanRegistration;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.MBeanServerDelegate;
import javax.management.MBeanServerNotification;
import javax.management.NotCompliantMBeanException;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.RuntimeOperationsException;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

import org.jboss.logging.Logger;
import org.jboss.mx.capability.DispatcherFactory;
import org.jboss.mx.metadata.MBeanCapability;
import org.jboss.mx.server.registry.MBeanEntry;
import org.jboss.mx.server.ServerObjectInstance;

/**
 * The registry for object name - object reference mapping in the
 * MBean server.
 * <p>
 * The implementation of this class affects the invocation speed
 * directly, please check any changes for performance.
 *
 * @todo JMI_DOMAIN isn't very protected
 *
 * @see org.jboss.mx.server.registry.MBeanRegistry
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @author  <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 * @version $Revision: 1.19.4.2 $
 */
public class BasicMBeanRegistry
   implements MBeanRegistry
{
   // Attributes ----------------------------------------------------

   /**
    * A map of domain name to another map containing object name canonical
    * key properties to registry entries.
    * domain -> canonicalKeyProperties -> MBeanEntry
    */
   private Map domainMap = new HashMap();

   /**
    * The default domain for this registry
    */
   private String defaultDomain;

   /**
    * The MBeanServer for which we are the registry.
    */
   private MBeanServer server = null;

   /**
    * Direct reference to the mandatory MBean server delegate MBean.
    */
   private MBeanServerDelegate delegate = null;

   /**
    * Sequence number for the MBean server registration notifications.
    */
   private long registrationNotificationSequence = 1;

   /**
    * Sequence number for the MBean server unregistration notifications.
    */
   private long unregistrationNotificationSequence = 1;

   // Static --------------------------------------------------------

   /**
    * The logger
    */
   private static Logger log = Logger.getLogger(BasicMBeanRegistry.class);

   // Constructors --------------------------------------------------

   /**
    * Constructs a new BasicMBeanRegistry.<p>
    *
    * @param the default domain of this registry.
    * @todo figure out why this is called twice on startup.
    */
   public BasicMBeanRegistry(MBeanServer server, String defaultDomain)
   {
      // Store the context
      this.server = server;
      this.defaultDomain = defaultDomain;
   }

   // MBeanRegistry Implementation ----------------------------------

   public ObjectInstance registerMBean(Object object, ObjectName name,
                                       Map valueMap)
      throws InstanceAlreadyExistsException,
             MBeanRegistrationException,
             NotCompliantMBeanException
   {
      boolean registrationDone = true;
      ObjectName regName = name;

      if (object == null)
         throw new RuntimeOperationsException(
               new IllegalArgumentException("Null object"));

      // Check the objects compliance
      MBeanCapability mbcap = MBeanCapability.of(object.getClass());

      // Does the bean require registration processing?
      MBeanRegistration registrationInterface = null;
      if (object instanceof MBeanRegistration)
      {
         registrationInterface = (MBeanRegistration) object;
      }

      try
      {
         // Do the preRegister, pass the fully qualified name
         if (null != registrationInterface)
         {
            if (regName != null)
               regName = qualifyName(regName);

            try
            {
               ObjectName mbean = registrationInterface.preRegister(server, regName);
               if (regName == null)
                  regName = mbean;
            }
            catch (Exception e)
            {
               // if the MBean throws MBeanRegistrationException, don't rewrap it
               if (e instanceof MBeanRegistrationException)
               {
                  throw e;
               }
               
               throw new MBeanRegistrationException(e, 
                     "preRegister() failed " +
                     "[ObjectName='" + name + 
                     "', Class=" + object.getClass().getName() +
                     " (" + object + ")]"
               );
            }
            catch (Throwable t)
            {
               // we can't wrap errors with MBeanRegistrationException
               
               log.warn("preRegister() failed for " + name + ": ", t);
               
               throw t;
            }
         }

         // This is the final check of the name
         String magicToken = null;

         if (valueMap != null)
         {
            magicToken = (String) valueMap.get(JMI_DOMAIN);
         }
         
         regName = validateAndQualifyName(regName, magicToken);

         try
         {
            // Register the mbean

            DynamicMBean invoker = null;
            if (mbcap.getMBeanType() == MBeanCapability.STANDARD_MBEAN)
               invoker = DispatcherFactory.create(mbcap.getStandardMBeanInfo(), object);
            else
               invoker = (DynamicMBean) object;

            MBeanEntry entry = new MBeanEntry(regName, invoker, object, valueMap);
            add(entry);

            try
            {
               long sequence;
               synchronized (this)
               {
                  sequence = registrationNotificationSequence++;
               }

               if (delegate != null)
               {
                  delegate.sendNotification(
                     new MBeanServerNotification(
                        MBeanServerNotification.REGISTRATION_NOTIFICATION,
                        delegate, sequence, regName));

               }

               else if (name.getCanonicalName().equals(MBEAN_SERVER_DELEGATE))
               {
                  delegate = (MBeanServerDelegate)object;
               }

               return new ServerObjectInstance(regName, entry.getResourceClassName(),
                                               delegate.getMBeanServerId());

            }
            catch (Throwable t)
            {
               // Problem, remove the mbean from the registry
               remove(regName);
               
               throw t;
            }
         }
         // Thrown by the registry
         catch (InstanceAlreadyExistsException e)
         {
            throw e;
         }
         catch (Throwable t)
         {
            // Something is broken
            log.error("Unexpected Exception:", t);
            
            throw t;
         }
      }
      catch (InstanceAlreadyExistsException e)
      {
         // It was already registered
         registrationDone = false;
         throw e;
      }
      catch (MBeanRegistrationException e)
      {
         // The MBean cancelled the registration
         registrationDone = false;

         log.warn(e.toString());
         
         throw e;
      }
      catch (RuntimeOperationsException e)
      {
         // There was a problem with one the arguments
         registrationDone = false;
         throw e;
      }
      catch (Throwable t)
      {
         // Some other error
         registrationDone = false;
         return null;
      }
      finally
      {
         // Tell the MBean the result of the registration
         if (registrationInterface != null)
            registrationInterface.postRegister(new Boolean(registrationDone));
      }
    }

   public void unregisterMBean(ObjectName name)
      throws InstanceNotFoundException, MBeanRegistrationException
   {
      name = qualifyName(name);
      if (name.getDomain().equals(JMI_DOMAIN))
         throw new RuntimeOperationsException(new IllegalArgumentException(
            "Not allowed to unregister: " + name.toString()));

      MBeanEntry entry = get(name);
      Object resource = entry.getResourceInstance();

      MBeanRegistration registrationInterface = null;

      if (resource instanceof MBeanRegistration)
      {
         registrationInterface = (MBeanRegistration) resource;
         try
         {
            registrationInterface.preDeregister();
         }
         catch (Exception e)
         {
            // don't double wrap MBeanRegistrationException
            if (e instanceof MBeanRegistrationException)
            {
               e.fillInStackTrace();
               throw (MBeanRegistrationException)e;
            }
            
            throw new MBeanRegistrationException(e, "preDeregister");
         }
      }

      // It is no longer registered
      remove(name);

      long sequence;
      synchronized (this)
      {
         sequence = unregistrationNotificationSequence++;
      }
      delegate.sendNotification(
         new MBeanServerNotification(
            MBeanServerNotification.UNREGISTRATION_NOTIFICATION,
            delegate,
            sequence,
            name
         )
      );

      if (registrationInterface != null)
         registrationInterface.postDeregister();
   }

   public synchronized MBeanEntry get(ObjectName name)
      throws InstanceNotFoundException
   {
      if (name == null)
         throw new InstanceNotFoundException("null object name");
         
      // Determine the domain and retrieve its entries
      String domain = name.getDomain();
      
      if (domain.length() == 0)
         domain = defaultDomain;
      
      String props = name.getCanonicalKeyPropertyListString();
      Map mbeanMap = (Map) domainMap.get(domain);

      // Retrieve the mbean entry
      Object o = null;
      if (null == mbeanMap || null == (o = mbeanMap.get(props)))
         throw new InstanceNotFoundException(name + " is not registered.");

      // We are done
      return (MBeanEntry) o;
   }

   public String getDefaultDomain()
   {
      return defaultDomain;
   }

   public synchronized ObjectInstance getObjectInstance(ObjectName name)
      throws InstanceNotFoundException
   {
      if (!contains(name))
         throw new InstanceNotFoundException(name + " not registered.");

      return new ServerObjectInstance(qualifyName(name),
         get(name).getResourceClassName(), delegate.getMBeanServerId());
   }

   public synchronized Object getValue(ObjectName name, String key)
      throws InstanceNotFoundException
   {
      return get(name).getValue(key);
   }

   public synchronized boolean contains(ObjectName name)
   {
      // null safety check
      if (name == null)
         return false;
         
      // Determine the domain and retrieve its entries
      String domain = name.getDomain();
      
      if (domain.length() == 0)
         domain = defaultDomain;
      
      String props = name.getCanonicalKeyPropertyListString();
      Map mbeanMap = (Map) domainMap.get(domain);

      // Return the result
      return (null != mbeanMap && mbeanMap.containsKey(props));
   }

   public synchronized int getSize()
   {
      int retval = 0;

      for (Iterator iterator = domainMap.values().iterator(); iterator.hasNext();)
      {
         retval += ((Map)iterator.next()).size();
      }
      return retval;
   }

   public synchronized List findEntries(ObjectName pattern)
   {
      ArrayList retval = new ArrayList();

      // There are a couple of shortcuts we can employ to make this a
      // bit faster - they're commented.

      // First, if pattern == null or pattern.getCanonicalName() == "*:*" we want the
      // set of all MBeans.
      if (pattern == null || pattern.getCanonicalName().equals("*:*"))
      {
         for (Iterator domainIter = domainMap.values().iterator(); domainIter.hasNext();)
         {
            for (Iterator mbeanIter = ((Map)domainIter.next()).values().iterator(); mbeanIter.hasNext();)
            {
               retval.add(mbeanIter.next());
            }
         }
      }
      // Next, if !pattern.isPattern() then we are doing a simple get (maybe defaultDomain).
      else if (!pattern.isPattern())
      {
         // simple get
         try
         {
            retval.add(get(pattern));
         }
         catch (InstanceNotFoundException e)
         {
            // we don't care
         }
      }
      // Now we have to do a brute force, oh well.
      else
      {
         String patternDomain = pattern.getDomain();
         if (patternDomain.length() == 0)
            patternDomain = defaultDomain;
         boolean patternIsPropertyPattern = pattern.isPropertyPattern();
         String patternCanonicalKPS = pattern.getCanonicalKeyPropertyListString();
         Object[] propkeys = null;
         Object[] propvals = null;

         // prebuild arrays of keys and values for quick comparison
         // when isPropertyPattern().
         if (patternIsPropertyPattern)
         {
            Hashtable patternKPList = pattern.getKeyPropertyList();
            propkeys = new Object[patternKPList.size()];
            propvals = new Object[propkeys.length];

            int i = 0;
            for (Iterator iterator = patternKPList.entrySet().iterator(); iterator.hasNext(); i++)
            {
               Map.Entry mapEntry = (Map.Entry) iterator.next();
               propkeys[i] = mapEntry.getKey();
               propvals[i] = mapEntry.getValue();
            }
         }

         // Here we go, step through every domain and see if our pattern matches before optionally checking
         // each ObjectName's properties for a match.
         for (Iterator domainIter = domainMap.entrySet().iterator(); domainIter.hasNext();)
         {
            Map.Entry mapEntry = (Map.Entry) domainIter.next();
            if (domainMatches((String) mapEntry.getKey(), patternDomain))
            {
               // yes it's a label, sue me.
               CHOOSE_MBEAN: for (Iterator mbeanIter = ((Map)mapEntry.getValue()).values().iterator(); mbeanIter.hasNext();)
               {
                  MBeanEntry entry = (MBeanEntry) mbeanIter.next();
                  ObjectName name = entry.getObjectName();

                  if (patternIsPropertyPattern)
                  {
                     // another shortcut - we only compare the key properties list of the registered MBean
                     // if properties have been specifed in the pattern (i.e. something other than just "*")
                     if (propkeys.length > 0)
                     {
                        Hashtable nameKPList = name.getKeyPropertyList();

                        for (int i = 0; i < propkeys.length; i++)
                        {
                           // skip the current MBean if there isn't a match
                           if ( !propvals[i].equals(nameKPList.get(propkeys[i])) )
                           {
                              continue CHOOSE_MBEAN;
                           }
                        }
                     }
                     retval.add(entry);
                  }
                  else
                  {
                     // Ok, it's a like-for-like comparison of the keyProperties.
                     // Knowing how our implementation of ObjectName works, it's *much*
                     // faster to compare the canonicalKeyProperties strings than it
                     // is to compare the two hashtables.
                     if (patternCanonicalKPS.equals(name.getCanonicalKeyPropertyListString()))
                     {
                        retval.add(entry);
                     }
                  }
               }
            }
         }
      }

      return retval;
   }

   /**
    * Compare the src and pat strings where ? and * chars are significant.
    *
    * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
    */
   protected boolean domainMatches(String src, String pat)
   {
      if (src.equals("*")) // no point doing more that we have to...
      {
         return true;
      }
      return domainMatches(src.toCharArray(), 0, pat.toCharArray(), 0);
   }

   /**
    * Compare the src and pat char arrays where ? and * chars are significant.
    *
    * I arrived at this solution after quite a bit of trial and error - it's
    * all a bit interwoven.  Obviously I'm no good at parsers and there must
    * be a clearer or more elegant way to do this.  I'm suitably in awe of
    * the perl regex hackers now.
    *
    * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
    */
   protected boolean domainMatches(char[] src, int spos, char[] pat, int ppos)
   {
      int slen = src.length;
      int plen = pat.length;

      while (ppos < plen)
      {
         char c = pat[ppos++];
         if ('?' == c)
         {
            // eat a src character and make sure we're not
            // already at the end
            if (spos++ == slen)
            {
               return false;
            }
         }
         else if ('*' == c)
         {
            if (ppos == plen) // shortcut - * at the end of the pattern
            {
               return true;
            }

            // hammer the src chars recursively until we
            // get a match or we drop off the end of src
            do
            {
               if (domainMatches(src, spos, pat, ppos))
               {
                  return true;
               }
            }
            while (++spos < slen);
         }
         else if (spos == slen || c != src[spos++])
         {
            return false;
         }
      }
      // fell through with no falses so make sure all of src was examined
      return (spos == slen);
   }

   /**
    * Adds an MBean entry<p>
    *
    * WARNING: The object name should be fully qualified.
    *
    * @param entry the MBean entry to add
    * @exception InstanceAlreadyExistsException when the MBean's object name
    *            is already registered
    */
   protected synchronized void add(MBeanEntry entry)
      throws InstanceAlreadyExistsException
   {
      // Determine the MBean's name and properties
      ObjectName name = entry.getObjectName();
      String domain = name.getDomain();
      String props = name.getCanonicalKeyPropertyListString();

      // Create a properties -> entry map if we don't have one
      Map mbeanMap = (Map) domainMap.get(domain);
      if (mbeanMap == null)
      {
         mbeanMap = new HashMap();
         domainMap.put(domain, mbeanMap);
      }

      // Make sure we aren't already registered
      if (mbeanMap.get(props) != null)
         throw new InstanceAlreadyExistsException(name + " already registered.");

      // Ok, we are registered
      mbeanMap.put(props, entry);
   }

   /**
    * Removes an MBean entry
    *
    * WARNING: The object name should be fully qualified.
    *
    * @param name the object name of the entry to remove
    * @exception InstanceNotFoundException when the object name is not
    *            registered
    */
   protected synchronized void remove(ObjectName name)
      throws InstanceNotFoundException
   {
      // Determine the MBean's name and properties
      String domain = name.getDomain();
      String props = name.getCanonicalKeyPropertyListString();
      Map mbeanMap = (Map) domainMap.get(domain);

      // Remove the entry, raise an exception when it didn't exist
      if (null == mbeanMap || null == mbeanMap.remove(props))
         throw new InstanceNotFoundException(name + " not registered.");
   }

   /**
    * Validates and qualifies an MBean<p>
    *
    * Validates the name is not a pattern.<p>
    *
    * Adds the default domain if no domain is specified.<p>
    *
    * Checks the name is not in the reserved domain JMImplementation when
    * the magicToken is not {@link org.jboss.mx.server.ServerConstants#JMI_DOMAIN JMI_DOMAIN}
    *
    * @param name the name to validate
    * @param magicToken used to get access to the reserved domain
    * @return the original name or the name prepended with the default domain
    *         if no domain is specified.
    * @exception RuntimeOperationException containing an
    *            IllegalArgumentException for a problem with the name
    */
   protected ObjectName validateAndQualifyName(ObjectName name,
                                               String magicToken)
   {
      // Check for qualification
      ObjectName result = qualifyName(name);

      // Make sure the name is not a pattern
      if (result.isPattern())
         throw new RuntimeOperationsException(
               new IllegalArgumentException("Object name is a pattern:" + name));

      // Check for reserved domain
      if (magicToken != JMI_DOMAIN &&
          result.getDomain().equals(JMI_DOMAIN))
         throw new RuntimeOperationsException(new IllegalArgumentException(
                     "Domain " + JMI_DOMAIN + " is reserved"));

      // I can't think of anymore tests, we're done
      return result;
   }

   /**
    * Qualify an object name with the default domain<p>
    *
    * Adds the default domain if no domain is specified.
    *
    * @param name the name to qualify
    * @return the original name or the name prepended with the default domain
    *         if no domain is specified.
    * @exception RuntimeOperationException containing an
    *            IllegalArgumentException when there is a problem
    */
   protected ObjectName qualifyName(ObjectName name)
   {
      if (name == null)
         throw new RuntimeOperationsException(
               new IllegalArgumentException("Null object name"));
      try
      {
         if (name.getDomain().length() == 0)
            return new ObjectName(defaultDomain + ":" +
                                  name.getCanonicalKeyPropertyListString());
         else
            return name;
      }
      catch (MalformedObjectNameException e)
      {
         throw new RuntimeOperationsException(
               new IllegalArgumentException(e.toString()));
      }
   }
   
}

