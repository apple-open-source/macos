/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.ejb;


import java.rmi.RemoteException;

import javax.ejb.EJBException;

import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;
import org.jboss.invocation.MarshalledValue;
import org.jboss.metadata.WebMetaData;

/**
 * Core implementation of methods for the bean.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession
 * @see org.jboss.ha.httpsession.beanimpl.ejb.ClusteredHTTPSessionBeanAbstract
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.4.5 $
 */
public abstract class ClusteredHTTPSessionBeanImpl extends ClusteredHTTPSessionBeanAbstract
{

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   protected SerializableHttpSession tmpSession = null;
   protected boolean isModified = false;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   // ClusteredHTTPSessionBeanAbstract overrides ---------------------------------------------------

   public void ejbStore () throws EJBException, RemoteException
   {
      if (tmpSession != null)
      {
         // the tmpSession has been assigned. Furthermore, if ejbStore is called
         // it means that isModified==true => we need to rebuild a serialized representation
         //
         serializeSession();

         this.tmpSession.sessionHasBeenStored ();
      }

      isModified = false;
   }

   public void ejbLoad () throws EJBException, RemoteException
   {
      // the tmp value is no more valid: a new serialized representation is just loaded.
      // it will be transformed only if explicitly asked.
      //
      tmpSession = null;
      isModified = false;
   }

   public SerializableHttpSession getSession ()
   {
      if (tmpSession == null)
      {
         // this is the first access to the object representation.
         // we use a lazy scheme => we unserialize now
         unserializeSession ();
      }
      return this.tmpSession;
   }

   public void setSession (SerializableHttpSession session)
   {
      if (tmpSession == null)
         isModified = true;
      else
         isModified = session.areAttributesModified (tmpSession);

      // in any case, we update the "time" attributes
      //
      this.setCreationTime (session.getContentCreationTime ());
      this.setLastAccessedTime (session.getContentLastAccessTime ());

      // in any cases, we assign the new session: this is because the session
      // may have internal data that is not used for the isModified comparison
      // (such as last access time). Consequently, if we use a load-balancer with
      // sticky sessions, the values will be kept in cache correctly whereas if we
      // don't have sticky session, these values will only be clustered-saved if
      // an attributed is modified!
      //
      this.tmpSession = session;
   }

    // Optimisation: called by the CMP engine
    //
    public boolean isModified () { return this.isModified; }

   public boolean useAsyncReplication ()
   {
      if (this.tmpSession != null)
         return tmpSession.getReplicationTypeForSession() == WebMetaData.REPLICATION_TYPE_ASYNC;
      else
         return true; // happens when a session is removed without unserializing it (lazy- (un-) serialization)
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   protected void serializeSession() throws EJBException
   {
      try
      {
         this.setSerializedSession (new MarshalledValue (this.tmpSession));
      }
      catch (Exception e)
      {
         throw new EJBException (e.toString ());
      }
   }

   protected void unserializeSession() throws EJBException
   {
      try
      {
         MarshalledValue mo = (MarshalledValue)this.getSerializedSession ();
         if (mo != null)
            this.tmpSession = (SerializableHttpSession)(mo.get ());
      }
      catch (Exception e)
      {
         throw new EJBException (e.toString ());
      }
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
