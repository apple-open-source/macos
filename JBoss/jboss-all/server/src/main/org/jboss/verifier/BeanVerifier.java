package org.jboss.verifier;

/*
 * Class org.jboss.verifier.BeanVerifier
 * Copyright (C) 2000  Juha Lindfors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This package and its source code is available at www.jboss.org
 * $Id: BeanVerifier.java,v 1.14.2.2 2002/09/19 09:54:34 lqd Exp $
 */

 
// standard imports
import java.util.Iterator;
import java.net.URL;

// non-standard class dependencies
import org.jboss.verifier.strategy.VerificationContext;
import org.jboss.verifier.strategy.VerificationStrategy;
import org.jboss.verifier.strategy.EJBVerifier11;
import org.jboss.verifier.strategy.EJBVerifier20;

import org.jboss.verifier.event.VerificationEvent;
import org.jboss.verifier.event.VerificationListener;
import org.jboss.verifier.event.VerificationEventGeneratorSupport;

import org.jboss.verifier.factory.VerificationEventFactory;

import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.SessionMetaData;
import org.jboss.metadata.MessageDrivenMetaData;

import org.jboss.logging.Logger;

/**
 * Attempts to verify the spec compliance of the beans in a given
 * EJB-JAR file. Works against EJB spec 1.1 and 2.0. Built for use in
 * JBoss project.
 *
 * @see     org.jboss.verifier.strategy.VerificationStrategy
 * @see     org.jboss.verifier.factory.VerificationEventFactory
 *
 * @author  <a href="mailto:juha.lindfors@jboss.org">Juha Lindfors</a>
 * @version $Revision: 1.14.2.2 $
 * @since   JDK 1.3
 */
public class BeanVerifier
   implements VerificationContext
{
   private ApplicationMetaData ejbMetaData = null;
   private URL ejbURL = null;
   private ClassLoader ejbClassLoader = null;

   private VerificationStrategy verifier = null;

   private boolean success = true;

   private static Logger log = Logger.getLogger( BeanVerifier.class );

   /*
    * Support class which handles the event notification logic.
    */
   private VerificationEventGeneratorSupport events =
      new VerificationEventGeneratorSupport();

   /**
    * Default constructor.
    */
   public BeanVerifier()
   {}

   /**
    * Checks the Enterprise Java Beans found in this Jar for EJB spec
    * compliance (EJB Spec. 1.1). Ensures that the given interfaces
    * and implementation classes implement required methods and follow
    * the contract given in the spec.
    *
    * @param   url     URL to the bean jar file
    */
   public void verify(URL url, ApplicationMetaData metaData)
   {
      verify(url, metaData, null);
   }

   /**
    * Checks the Enterprise Java Beans found in this Jar for EJB spec
    * compliance (EJB Spec. 1.1). Ensures that the given interfaces
    * and implementation classes implement required methods and follow
    * the contract given in the spec.
    *
    * @param   url     URL to the bean jar file
    * @param   cl      The ClassLoader to use
    */
   public void verify(URL url, ApplicationMetaData metaData, ClassLoader cl)
   {
      ejbURL = url;
      ejbMetaData = metaData;
      ejbClassLoader = cl;

      if(metaData.isEJB1x())
      {
         setVerifier(VERSION_1_1);
      }
      else
      {
         setVerifier(VERSION_2_0);
      }

      Iterator beans = ejbMetaData.getEnterpriseBeans();

      while (beans.hasNext())
      {
         BeanMetaData bean = (BeanMetaData)beans.next();

         if( bean.isEntity() )
         {
            EntityMetaData entityBean = (EntityMetaData)bean;
            if( metaData.isEJB2x() && entityBean.isCMP1x() )
            {
               // Hook for verifying CMP 1.x Beans in a 2.x JAR: store
               // current state and restore this state after verification
               // of the EJB completes.
               boolean storedSuccess = success;

               verifier.checkEntity( entityBean );

               if( success != storedSuccess )
               {
                  log.warn( "The CMP 1.x EJB '" + entityBean.getEjbName() +
                     "' generated some verification warnings. The Deployer " +
                     "will ignore these warnings but you should check " +
                     "your EJB to be on the safe side of things." );
               }

               success = storedSuccess;
            }
            else
            {
               verifier.checkEntity( entityBean );
            }
         }
         else if( bean.isSession() )
         {
            verifier.checkSession( (SessionMetaData)bean );
         }
         else
         {
            verifier.checkMessageBean( (MessageDrivenMetaData)bean );
         }
      }
   }

   /**
    * Check if the Verifier was successful
    *
    * @return <code>true</code> if all Beans have been verified,
    *   <code>false</code> otherwise.
    */
   public boolean getSuccess()
   {
      return success;
   }

   /*
    *************************************************************************
    *
    *   IMPLEMENTS VERIFICATION EVENT GENERATOR INTERFACE
    *
    *************************************************************************
    */
   public void addVerificationListener(VerificationListener listener)
   {
      events.addVerificationListener(listener);
   }

   public void removeVerificationListener(VerificationListener listener)
   {
      events.removeVerificationListener(listener);
   }

   public void fireBeanChecked(VerificationEvent event)
   {
      events.fireBeanChecked(event);
   }

   public void fireSpecViolation( VerificationEvent event )
   {
      // A Spec Violation has been found. Mark as unsuccessful.
      success = false;
      events.fireSpecViolation(event);
   }

   /*
    **************************************************************************
    *
    *   IMPLEMENTS VERIFICATION CONTEXT INTERFACE
    *
    **************************************************************************
    */
   public ApplicationMetaData getApplicationMetaData()
   {
      return ejbMetaData;
   }

   public URL getJarLocation()
   {
      return ejbURL;
   }

   public ClassLoader getClassLoader()
   {
      return ejbClassLoader;
   }

   public String getEJBVersion()
   {
      return VERSION_1_1;

      // [TODO] fix this to return a correct version
   }

   /*
    * Will set the correct strategy implementation according to the supplied
    * version information. Might widen the scope to public, but protected
    * will do for now.
    */
   protected void setVerifier( String version )
   {
      if( VERSION_1_1.equals(version) )
      {
         verifier = new EJBVerifier11(this);
      }
      else if( VERSION_2_0.equals(version) )
      {
         verifier = new EJBVerifier20(this);
      }
      else
      {
         throw new IllegalArgumentException( UNRECOGNIZED_VERSION +
            ": " + version);
      }
   }

   /*
    * accessor for reference to the verification strategy in use
    */
   protected VerificationStrategy getVerifier()
   {
      return verifier;
   }

   /*
    * String constants
    */
   private final static String UNRECOGNIZED_VERSION =
      "Unknown version string";
}
/*
vim:ts=3:sw=3:et
*/
