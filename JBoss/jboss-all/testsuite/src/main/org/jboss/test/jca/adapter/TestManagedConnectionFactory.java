
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.adapter; 

import java.io.PrintWriter;
import java.net.URL;
import java.util.Set;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

// Generated package name
/**
 * ManagedConnectionFactory.java
 *
 *
 * Created: Mon Dec 31 17:01:55 2001
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestManagedConnectionFactory implements ManagedConnectionFactory
{
   //number the managed connections
   int id;

   String failure;

   public TestManagedConnectionFactory ()
   {
      
   }

   public void setFailure(String failure)
   {
      this.failure = failure;
   }

   // implementation of javax.resource.spi.ManagedConnectionFactory interface

   /**
    *
    * @return <description>
    */
   public int hashCode()
   {
     // TODO: implement this javax.resource.spi.ManagedConnectionFactory method
     return 0;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    */
   public boolean equals(Object other)
   {
      return (other != null) && (other.getClass() == getClass());
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.resource.ResourceException <description>
    */
   public void setLogWriter(PrintWriter param1) throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnectionFactory method
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public PrintWriter getLogWriter() throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnectionFactory method
     return null;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Object createConnectionFactory(ConnectionManager cm) throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnectionFactory method
     return new TestConnectionFactory(cm, this);
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Object createConnectionFactory() throws ResourceException
   {
      throw new ResourceException("not yet implemented");
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ManagedConnection createManagedConnection(Subject subject, ConnectionRequestInfo cri) throws ResourceException
   {
      if (failure != null && failure.equals("createManagedConnectionResource"))
         throw new ResourceException("");
      if (failure != null && failure.equals("createManagedConnectionRuntime"))
         throw new RuntimeException("");
      return new TestManagedConnection(subject, (TestConnectionRequestInfo)cri, id++);
   }

   /**
    * Describe <code>matchManagedConnections</code> method here.
    *
    * @param candidates a <code>Set</code> value
    * @param subject a <code>Subject</code> value
    * @param cri a <code>ConnectionRequestInfo</code> value
    * @return a <code>ManagedConnection</code> value
    * @exception ResourceException if an error occurs
    */
   public ManagedConnection matchManagedConnections(Set candidates, Subject subject, ConnectionRequestInfo cri) throws ResourceException
   {
      if (failure != null && failure.equals("matchManagedConnectionResource"))
         throw new ResourceException("");
      if (failure != null && failure.equals("matchManagedConnectionRuntime"))
         throw new RuntimeException("");
      if (candidates.isEmpty()) 
      {
         return null;
      } // end of if ()
      return (ManagedConnection)candidates.iterator().next();
   }

   Integer integerProperty;

   /**
    * Get the IntegerProperty value.
    * @return the IntegerProperty value.
    */
   public Integer getIntegerProperty()
   {
      return integerProperty;
   }

   /**
    * Set the IntegerProperty value.
    * @param newIntegerProperty The new IntegerProperty value.
    */
   public void setIntegerProperty(Integer integerProperty)
   {
      this.integerProperty = integerProperty;
   }

   Integer defaultIntegerProperty;

   /**
    * Get the DefaultIntegerProperty value.
    * @return the DefaultIntegerProperty value.
    */
   public Integer getDefaultIntegerProperty()
   {
      return defaultIntegerProperty;
   }

   /**
    * Set the DefaultIntegerProperty value.
    * @param newDefaultIntegerProperty The new DefaultIntegerProperty value.
    */
   public void setDefaultIntegerProperty(Integer defaultIntegerProperty)
   {
      this.defaultIntegerProperty = defaultIntegerProperty;
   }

   Boolean booleanProperty;

   /**
    * Get the BooleanProperty value.
    * @return the BooleanProperty value.
    */
   public Boolean getBooleanProperty()
   {
      return booleanProperty;
   }

   /**
    * Set the BooleanProperty value.
    * @param newBooleanProperty The new BooleanProperty value.
    */
   public void setBooleanProperty(Boolean booleanProperty)
   {
      this.booleanProperty = booleanProperty;
   }

   Long longProperty;

   /**
    * Get the LongProperty value.
    * @return the LongProperty value.
    */
   public Long getLongProperty()
   {
      return longProperty;
   }

   /**
    * Set the LongProperty value.
    * @param newLongProperty The new LongProperty value.
    */
   public void setLongProperty(Long longProperty)
   {
      this.longProperty = longProperty;
   }

   Double doubleProperty;

   /**
    * Get the DoubleProperty value.
    * @return the DoubleProperty value.
    */
   public Double getDoubleProperty()
   {
      return doubleProperty;
   }

   /**
    * Set the DoubleProperty value.
    * @param newDoubleProperty The new DoubleProperty value.
    */
   public void setDoubleProperty(Double doubleProperty)
   {
      this.doubleProperty = doubleProperty;
   }

   URL urlProperty;

   /**
    * Get the UrlProperty value. (this is a jboss specific property editor)
    * @return the UrlProperty value.
    */
   public URL getUrlProperty()
   {
      return urlProperty;
   }

   /**
    * Set the UrlProperty value.
    * @param newUrlProperty The new UrlProperty value.
    */
   public void setUrlProperty(URL urlProperty)
   {
      this.urlProperty = urlProperty;
   }

   

}
