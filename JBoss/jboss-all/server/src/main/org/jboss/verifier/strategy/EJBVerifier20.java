package org.jboss.verifier.strategy;
/*
 * Class org.jboss.verifier.strategy.EJBVerifier20
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
 * $Id: EJBVerifier20.java,v 1.24.2.9 2003/10/24 13:00:13 loubyansky Exp $
 */


// standard imports
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.lang.reflect.Method;

// non-standard class dependencies
import org.jboss.logging.Logger;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.MessageDrivenMetaData;
import org.jboss.metadata.QueryMetaData;
import org.jboss.metadata.SessionMetaData;
import org.jboss.util.Classes;
import org.jboss.verifier.Section;
import org.jboss.verifier.factory.DefaultEventFactory;

/**
 * EJB 2.0 bean verifier.
 *
 * @author Juha Lindfors   (jplindfo@helsinki.fi)
 * @author Jay Walters     (jwalters@computer.org)
 * @author <a href="mailto:criege@riege.com">Christian Riege</a>
 *
 * @version $Revision: 1.24.2.9 $
 * @since   JDK 1.3
 */
public class EJBVerifier20
   extends AbstractVerifier
{
   private static Logger log = Logger.getLogger(EJBVerifier20.class);
   private static final String msgBundle = "EJB20Messages.properties";

   protected EJBVerifier11 cmp1XVerifier;

   // The classes for an EJB
   protected Class bean;
   protected Class home;
   protected Class remote;
   protected Class localHome;
   protected Class local;

   /*
    * Constructor
    */
   public EJBVerifier20(VerificationContext context)
   {
      super( context, new DefaultEventFactory(msgBundle) );
      cmp1XVerifier = new EJBVerifier11(context);
   }

/*
 ***********************************************************************
 *
 *    IMPLEMENTS VERIFICATION STRATEGY INTERFACE
 *
 ***********************************************************************
 */
   public void checkSession( SessionMetaData session )
   {
      boolean localOrRemoteExists = false;
      boolean verified = false;

      if( !verifyBean(session) )
         return;

      verified = verifySessionBean( session );

      if( hasRemoteInterfaces(session) )
      {
         // Check remote interfaces
         localOrRemoteExists = true;
         verified = verified && verifySessionRemote( session );
         verified = verified && verifySessionHome( session );
      }

      if( hasLocalInterfaces(session) )
      {
         // Check local interfaces
         localOrRemoteExists = true;
         verified = verified && verifySessionLocal( session );
         verified = verified && verifySessionLocalHome( session );
      }

      // The session bean MUST implement either a remote home and
      // remote, or local home and local interface.  It MAY implement a
      // remote home, remote, local home or local interface.
      //
      // Spec 7.10.1
      //
      if( !localOrRemoteExists )
      {
         fireSpecViolationEvent( session, new Section("7.10.1") );
         verified = false;
      }

      if( verified )
      {
         // All OK; full steam ahead
         fireBeanVerifiedEvent( session );
      }
   }

   public void checkEntity( EntityMetaData entity )
   {
      if (entity.isCMP1x())
      {
         cmp1XVerifier.checkEntity( entity );
      }
      else
      {
         checkBmpOrCmp2Entity( entity );
      }
   }

   public void checkMessageBean( MessageDrivenMetaData mdb )
   {
      boolean beanVerified   = false;

      if( !verifyBean(mdb) )
         return;

      beanVerified = verifyMessageDrivenBean( mdb );

      if (beanVerified)
      {
         // OK, we're done
         fireBeanVerifiedEvent( mdb );
      }
   }

   private void checkBmpOrCmp2Entity( EntityMetaData entity )
   {
      boolean localOrRemoteExists = false;
      boolean verified = false;

      if( !verifyBean(entity) )
         return;

      if( entity.isCMP() )
      {
         verified = verifyCMPEntityBean(entity);
      }
      else if( entity.isBMP() )
      {
         verified = verifyBMPEntityBean(entity);
      }

      if( hasRemoteInterfaces(entity) )
      {
         // Check remote interfaces
         localOrRemoteExists = true;
         verified = verified && verifyEntityRemote( entity );
         verified = verified && verifyEntityHome( entity );
      }

      if( hasLocalInterfaces(entity) )
      {
         // Check local interfaces
         localOrRemoteExists = true;
         verified = verified && verifyEntityLocal( entity );
         verified = verified && verifyEntityLocalHome( entity );
      }

      verified = verified && verifyPrimaryKey( entity );

      if( !localOrRemoteExists )
      {
         // The entity bean MUST implement either a remote home and
         // remote, or local home and local interface.  It MAY implement
         // a remote home, remote, local home or local interface.
         //
         // Spec 10.6.1 (CMP) / 12.2.1 (BMP)
         //
         if( entity.isCMP() )
         {
            fireSpecViolationEvent(entity, new Section("10.6.1"));
            verified = false;
         }
         else
         {
            fireSpecViolationEvent(entity, new Section("12.2.1"));
            verified = false;
         }
      }

      if( verified )
      {
         fireBeanVerifiedEvent(entity);
      }
   }

   /**
    * Try to load the beans class declared in the &lt;ejb-class&gt;
    * element.
    *
    * @return <code>true</code> if everything went alright
    */
   protected boolean verifyBean( BeanMetaData theBean )
   {
      String beanName = theBean.getEjbClass();

      if( beanName == null )
         return false;

      try
      {
         bean = classloader.loadClass( beanName );
         return true;
      }
      catch( ClassNotFoundException cnfe )
      {
         fireSpecViolationEvent( theBean, new Section("22.2.b",
            "Class not found on '" + beanName + "': " + cnfe.getMessage()) );
         return false;
      }
   }

   /**
    * Check whether the bean has declared local interfaces and whether
    * we can load the defined classes
    *
    * @return <code>true</code> if everything went alright
    */
   protected boolean hasRemoteInterfaces( BeanMetaData bean )
   {
      boolean status = true;
      String homeName = bean.getHome();
      String remoteName = bean.getRemote();

      if( homeName == null || remoteName == null )
         return false;

      // Verify the <home> class
      try
      {
         home = classloader.loadClass( homeName );
      }
      catch( ClassNotFoundException cnfe )
      {
         fireSpecViolationEvent( bean, new Section("22.2.c",
            "Class not found on '" + homeName + "': " + cnfe.getMessage()) );
         status = false;
      }

      // Verify the <remote> class
      try
      {
         remote = classloader.loadClass( remoteName );
      }
      catch( ClassNotFoundException cnfe )
      {
         fireSpecViolationEvent( bean, new Section("22.2.d",
            "Class not found on '" + remoteName + "': " + cnfe.getMessage()) );
         status = false;
      }

      return status;
   }

   /**
    * Check whether the bean has declared local interfaces and whether
    * we can load the defined classes
    *
    * @return <code>true</code> if everything went alright
    */
   protected boolean hasLocalInterfaces( BeanMetaData bean )
   {
      boolean status = true;
      String localHomeName = bean.getLocalHome();
      String localName = bean.getLocal();

      if( localHomeName == null || localName == null )
         return false;

      // Verify the <local-home> class
      try
      {
         localHome = classloader.loadClass( localHomeName );
      }
      catch( ClassNotFoundException cnfe )
      {
         fireSpecViolationEvent( bean, new Section("22.2.e",
            "Class not found on '" + localHomeName + "': " +
            cnfe.getMessage()) );
         status = false;
      }

      try
      {
         local = classloader.loadClass( localName );
      }
      catch( ClassNotFoundException cnfe )
      {
         fireSpecViolationEvent( bean, new Section("22.2.f",
            "Class not found on '" + localName + "': " + cnfe.getMessage()) );
         status = false;
      }

      return status;
   }

   /**
    * Check whether the given method is a create(...) method
    */
   public boolean isCreateMethod(Method m)
   {
      return m.getName().startsWith( CREATE_METHOD );
   }

   public boolean isEjbCreateMethod(Method m)
   {
      return m.getName().startsWith( EJB_CREATE_METHOD );
   }

   public boolean isEjbRemoveMethod(Method m)
   {
      return m.getName().startsWith( EJB_REMOVE_METHOD );
   }

   public boolean isEjbSelectMethod(Method m)
   {
      return m.getName().startsWith( EJB_SELECT_METHOD );
   }

   public boolean isEjbHomeMethod(Method m)
   {
      return m.getName().startsWith( EJB_HOME_METHOD );
   }

   /**
    * Return all ejbSelect methods
    */
   public Iterator getEjbSelectMethods( Class c )
   {
      List selects = new LinkedList();
      Method[] method = c.getMethods();

      for (int i = 0; i < method.length; ++i)
      {
         if (isEjbSelectMethod(method[i]))
         {
            selects.add(method[i]);
         }
      }

      return selects.iterator();
   }

   /**
    * Searches for an instance of an ejbRemove method from the class
    */
   public boolean hasEJBRemoveMethod( Class c )
   {
      Method[] method = c.getMethods();
      for (int i = 0; i < method.length; ++i)
      {
         if( isEjbRemoveMethod(method[i]) )
            return true;
      }

      return false;
   }

   /**
    * Returns the ejbRemove(...) methods of a bean
    */
   public Iterator getEJBRemoveMethods( Class c )
   {
      List ejbRemoves = new LinkedList();
      Method[] method = c.getMethods();

      for( int i = 0; i < method.length; ++i )
      {
         if (isEjbRemoveMethod(method[i]))
            ejbRemoves.add(method[i]);
      }

      return ejbRemoves.iterator();
   }

   /**
    * Home methods are any method on the home interface which is
    * neither a create or find method.
    */
   public Iterator getHomeMethods( Class c )
   {
      List homes = new LinkedList();
      Method[] method = c.getMethods();

      for (int i = 0; i < method.length; ++i)
      {
         if( !isCreateMethod(method[i]) && !isFinderMethod(method[i]))
            homes.add(method[i]);
      }

      return homes.iterator();
   }

   public Iterator getEjbHomeMethods( Class c )
   {
      List homes = new LinkedList();
      Method[] method = c.getMethods();

      for (int i = 0; i < method.length; ++i)
      {
         if (isEjbHomeMethod(method[i]))
            homes.add(method[i]);
      }

      return homes.iterator();
   }

   /**
    * Check whether there is a matching &lt;query&gt; Element defined
    * for the Method m
    *
    * @param m Method to check, should be either a Finder or a Select
    * @param e EntityMetaData
    *
    * @return <code>true</code> if a matching &lt;query&gt; Element
    *         was located.
    */
   protected boolean hasMatchingQuery( Method m, EntityMetaData e )
   {
      boolean result = false;

      Iterator qIt = e.getQueries();
      while( qIt.hasNext() )
      {
         QueryMetaData qmd = (QueryMetaData)qIt.next();

         // matching names
         if( !qmd.getMethodName().equals(m.getName()) )
         {
            continue;
         }

         Class[] methodParameter = m.getParameterTypes();
         Class[] queryParameter = null;

         try
         {
            queryParameter = Classes.convertToJavaClasses(
               qmd.getMethodParams(), classloader );
         }
         catch( ClassNotFoundException cnfe )
         {
            // FIXME: this should be handled differently, especially
            //        there shouldn't be a DeploymentException being
            //        thrown ...
            continue;
         }

         // number of parameters has to match
         if( methodParameter.length != queryParameter.length )
         {
            continue;
         }

         // walk the parameter list and compare for equality
         boolean parametersMatch = true;
         for( int i = 0; i < methodParameter.length; i++ )
         {
            if( !methodParameter[i].equals(queryParameter[i]) )
            {
               parametersMatch = false;
               break;
            }
         }

         if( parametersMatch )
         {
            result = true;
            break;
         }
      }

      return result;
   }

   /**
    * Verifies the session bean remote home interface against the EJB 2.0
    * specification.
    *
    * @param   session     XML metadata of the session bean
    */
   private boolean verifySessionHome(SessionMetaData session)
   {
      boolean status = true;

       // The home interface of a stateless session bean MUST have one
       // create() method that takes no arguments.
       //
       // The create() method MUST return the session bean's remote
       // interface.
       //
       // There CAN NOT be other create() methods in the home interface.
       //
       // Spec 7.10.6
       //
      if (session.isStateless())
      {
         if (!hasDefaultCreateMethod(home))
         {
            fireSpecViolationEvent(session, new Section("7.10.6.d2"));
            status = false;
         }
         else
         {
            Method create = getDefaultCreateMethod(home);

            if (hasMoreThanOneCreateMethods(home))
            {
               fireSpecViolationEvent(session, new Section("7.10.6.d2"));
               status = false;
            }
         }
      }

      // The session bean's home interface MUST extend the
      // javax.ejb.EJBHome interface.
      //
      // Spec 7.10.6
      //
      if (!hasEJBHomeInterface(home))
      {
         fireSpecViolationEvent(session, new Section("7.10.6.a"));
         status = false;
      }

      // Method arguments defined in the home interface MUST be
      // of valid types for RMI/IIOP.
      //
      // Method return values defined in the home interface MUST
      // be of valid types for RMI/IIOP.
      //
      // Methods defined in the home interface MUST include
      // java.rmi.RemoteException in their throws clause.
      //
      // Spec 7.10.6
      ///
      Iterator it = Arrays.asList(home.getMethods()).iterator();
      while (it.hasNext())
      {
         Method method = (Method)it.next();

         if (!hasLegalRMIIIOPArguments(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.6.b1"));
            status = false;
         }

         if (!hasLegalRMIIIOPReturnType(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.6.b2"));
            status = false;
         }

         if (!throwsRemoteException(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.6.b3"));
            status = false;
         }
      }

      // A session bean's home interface MUST define one or more
      // create(...) methods.
      //
      // Spec 7.10.6
      //
      if (!hasCreateMethod(home))
      {
         fireSpecViolationEvent(session, new Section("7.10.6.d1"));
         status = false;
      }

      // Each create(...) method in the session bean's home interface
      // MUST have a matching ejbCreate(...) method in the session
      // bean's class.
      //
      // Each create(...) method in the session bean's home interface
      // MUST have the same number and types of arguments to its
      // matching ejbCreate(...) method.
      //
      // The return type for a create(...) method MUST be the session
      // bean's remote interface type.
      //
      // All the exceptions defined in the throws clause of the matching
      // ejbCreate(...) method of the enterprise bean class MUST be
      // included in the throws clause of a matching create(...) method.
      //
      // The throws clause of a create(...) method MUST include the
      // javax.ejb.CreateException.
      //
      // Spec 7.10.6
      //
      Iterator createMethods = getCreateMethods(home);
      while (createMethods.hasNext())
      {
         Method create = (Method)createMethods.next();

         if (!hasMatchingEJBCreate(bean, create))
         {
            fireSpecViolationEvent(session, create, new Section("7.10.6.e"));
            status = false;
         }

         if (!hasRemoteReturnType(session, create))
         {
            fireSpecViolationEvent(session, create, new Section("7.10.6.f"));
            status = false;
         }

         if (hasMatchingEJBCreate(bean, create))
         {
            Method ejbCreate     = getMatchingEJBCreate(bean, create);
            if (!hasMatchingExceptions(ejbCreate, create))
            {
               fireSpecViolationEvent(session, create,
                  new Section("7.10.6.g"));
               status = false;
            }
         }

         if (!throwsCreateException(create))
         {
            fireSpecViolationEvent(session, create, new Section("7.10.6.h"));
            status = false;
         }
      }

      return status;
   }

   /**
    * Verifies the session bean local home interface against the EJB 2.0
    * specification.
    *
    * @param session parsed metadata of the session bean
    */
   private boolean verifySessionLocalHome(SessionMetaData session)
   {
      boolean status = true;

      // The local home interface of a stateless session bean MUST have
      // one create() method that takes no arguments.
      //
      // There CAN NOT be other create() methods in the home interface.
      //
      // Spec 7.10.8
      //
      if (session.isStateless())
      {
         if( !hasDefaultCreateMethod(localHome) )
         {
            fireSpecViolationEvent(session, new Section("7.10.8.d2"));
            status = false;
         }
         else
         {
            Method create = getDefaultCreateMethod( localHome );

            if( hasMoreThanOneCreateMethods(localHome) )
            {
               fireSpecViolationEvent(session, new Section("7.10.8.d2"));
               status = false;
            }
         }
      }

      // The session bean's home interface MUST extend the
      // javax.ejb.EJBLocalHome interface.
      //
      // Spec 7.10.8
      //
      if( !hasEJBLocalHomeInterface(localHome) )
      {
         fireSpecViolationEvent(session, new Section("7.10.8.a"));
         status = false;
      }

      // Methods defined in the local home interface MUST NOT include
      // java.rmi.RemoteException in their throws clause.
      //
      // Spec 7.10.8
      //
      Iterator it = Arrays.asList(localHome.getMethods()).iterator();
      while (it.hasNext())
      {
         Method method = (Method)it.next();

         if (throwsRemoteException(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.8.b"));
            status = false;
         }
      }

      // A session bean's home interface MUST define one or more
      // create(...) methods.
      //
      // Spec 7.10.8
      //
      if( !hasCreateMethod(localHome) )
      {
         fireSpecViolationEvent(session, new Section("7.10.8.d1"));
         status = false;
      }

      // Each create(...) method in the session bean's local home
      // interface MUST have a matching ejbCreate(...) method in the
      // session bean's class.
      //
      // Each create(...) method in the session bean's home interface
      // MUST have the same number and types of arguments to its
      // matching ejbCreate(...) method.
      //
      // The return type for a create(...) method MUST be the session
      // bean's local interface type.
      //
      // All the exceptions defined in the throws clause of the matching
      // ejbCreate(...) method of the enterprise bean class MUST be
      // included in the throws clause of a matching create(...) method.
      //
      // The throws clause of a create(...) method MUST include the
      // javax.ejb.CreateException.
      //
      // Spec 7.10.8
      //
      Iterator createMethods = getCreateMethods(localHome);
      while (createMethods.hasNext())
      {
         Method create = (Method)createMethods.next();

         if (!hasMatchingEJBCreate(bean, create))
         {
            fireSpecViolationEvent(session, create,
                  new Section("7.10.8.e"));
            status = false;
         }

         if (!hasLocalReturnType(session, create))
         {
            fireSpecViolationEvent(session, create,
               new Section("7.10.8.f"));
            status = false;
         }

         if (hasMatchingEJBCreate(bean, create))
         {
            Method ejbCreate = getMatchingEJBCreate(bean, create);
            if (!hasMatchingExceptions(ejbCreate, create))
            {
               fireSpecViolationEvent(session, create,
                  new Section("7.10.8.g"));
            }
         }

         if (!throwsCreateException(create))
         {
            fireSpecViolationEvent(session, create,
               new Section("7.10.8.h"));
            status = false;
         }
      }

      return status;
   }

   /*
    * Verify Session Bean Remote Interface
    */
   private boolean verifySessionRemote(SessionMetaData session)
   {
      boolean status = true;

      // The remote interface MUST extend the javax.ejb.EJBObject
      // interface.
      //
      // Spec 7.10.5
      //
      if (!hasEJBObjectInterface(remote))
      {
         fireSpecViolationEvent(session, new Section("7.10.5.a"));
         status = false;
      }

      // Method arguments defined in the remote interface MUST be
      // of valid types for RMI/IIOP.
      //
      // Method return values defined in the remote interface MUST
      // be of valid types for RMI/IIOP.
      //
      // Methods defined in the remote interface MUST include
      // java.rmi.RemoteException in their throws clause.
      //
      // Spec 7.10.5
      //
      Iterator it = Arrays.asList(remote.getMethods()).iterator();
      while (it.hasNext())
      {
         Method method = (Method)it.next();

         if (!hasLegalRMIIIOPArguments(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.5.b1"));
            status = false;
         }

         if (!hasLegalRMIIIOPReturnType(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.5.b2"));
            status = false;
         }

         if (!throwsRemoteException(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.5.b3"));
            status = false;
         }
      }

      // For each method defined in the remote interface, there MUST be
      // a matching method in the session bean's class. The matching
      // method MUST have:
      //
      //  - the same name
      //  - the same number and types of arguments, and the same
      //    return type
      //  - All the exceptions defined in the throws clause of the
      //    matching method of the session bean class must be defined
      //    in the throws clause of the method of the remote interface
      //
      // Spec 7.10.5
      //
      it = Arrays.asList(remote.getDeclaredMethods()).iterator();
      while (it.hasNext())
      {
         Method remoteMethod  = (Method)it.next();

         if (!hasMatchingMethod(bean, remoteMethod))
         {
            fireSpecViolationEvent( session, remoteMethod,
               new Section("7.10.5.d1"));

            status = false;
         }

         if (hasMatchingMethod(bean, remoteMethod))
         {
            try
            {
               Method beanMethod = bean.getMethod( remoteMethod.getName(),
                  remoteMethod.getParameterTypes());

               if (!hasMatchingReturnType(remoteMethod, beanMethod))
               {
                  fireSpecViolationEvent( session, remoteMethod,
                     new Section("7.10.5.d2"));
                  status = false;
               }

               if (!hasMatchingExceptions(beanMethod, remoteMethod))
               {
                  fireSpecViolationEvent( session, remoteMethod,
                     new Section("7.10.5.d3"));
                  status = false;
               }
            } catch (NoSuchMethodException ignored)
            {}
         }
      }

      return status;
   }

   /*
    * Verify Session Bean Local Interface
    */
   private boolean verifySessionLocal(SessionMetaData session)
   {
      boolean status = true;

      // The local interface MUST extend the javax.ejb.EJBLocalObject
      // interface.
      //
      // Spec 7.10.7
      //
      if (!hasEJBLocalObjectInterface(local))
      {
         fireSpecViolationEvent(session, new Section("7.10.7.a"));
         status = false;
      }

      // Methods defined in the local interface MUST NOT include
      // java.rmi.RemoteException in their throws clause.
      //
      // Spec 7.10.7
      //
      Iterator it = Arrays.asList(local.getMethods()).iterator();
      while (it.hasNext())
      {
         Method method = (Method)it.next();
         if (throwsRemoteException(method))
         {
            fireSpecViolationEvent(session, method, new Section("7.10.7.b"));
            status = false;
         }
      }

      // For each method defined in the local interface, there MUST be
      // a matching method in the session bean's class. The matching
      // method MUST have:
      //
      //  - the same name
      //  - the same number and types of arguments, and the same
      //    return type
      //  - All the exceptions defined in the throws clause of the
      //    matching method of the session bean class must be defined
      //    in the throws clause of the method of the remote interface
      //
      // Spec 7.10.7
      //
      it = Arrays.asList(local.getDeclaredMethods()).iterator();
      while (it.hasNext())
      {
         Method localMethod  = (Method)it.next();

         if (!hasMatchingMethod(bean, localMethod))
         {
            fireSpecViolationEvent( session, localMethod,
               new Section("7.10.7.d1"));
            status = false;
         }

         if (hasMatchingMethod(bean, localMethod))
         {
            try
            {
               Method beanMethod = bean.getMethod( localMethod.getName(),
                  localMethod.getParameterTypes());

               if (!hasMatchingReturnType(localMethod, beanMethod))
               {
                  fireSpecViolationEvent( session, localMethod,
                     new Section("7.10.7.d2"));
               status = false;
               }

               if (!hasMatchingExceptions(beanMethod, localMethod))
               {
                  fireSpecViolationEvent( session, localMethod,
                     new Section("7.10.7.d3"));
                  status = false;
               }
            } catch (NoSuchMethodException ignored)
            {}
         }
      }

      return status;
   }

   /*
    * Verify Session Bean
    */
   private boolean verifySessionBean(SessionMetaData session)
   {
      boolean status = true;

      // A session bean MUST implement, directly or indirectly,
      // javax.ejb.SessionBean interface.
      //
      // Spec 7.10.2
      //
      if (!hasSessionBeanInterface(bean))
      {
         fireSpecViolationEvent(session, new Section("7.10.2.a"));
         status = false;
      }

      // Only a stateful container-managed transaction demarcation
      // session bean MAY implement the SessionSynchronization
      // interface.
      //
      // A stateless Session bean MUST NOT implement the
      // SessionSynchronization interface.
      //
      // Spec 7.5.3
      //
      if( hasSessionSynchronizationInterface(bean) )
      {
         if (session.isStateless())
         {
            fireSpecViolationEvent(session, new Section("7.5.3.a"));
            status = false;
         }

         if (session.isBeanManagedTx())
         {
            fireSpecViolationEvent(session, new Section("7.5.3.b"));
            status = false;
         }
      }

      //
      // A session bean MUST implement AT LEAST one ejbCreate method.
      //
      // Spec 7.10.3
      //
      if (!hasEJBCreateMethod(bean, true))
      {
         fireSpecViolationEvent(session, new Section("7.10.3"));
         status = false;
      }

      // A session with bean-managed transaction demarcation CANNOT
      // implement the SessionSynchronization interface.
      //
      // Spec 7.6.1 (table 2)
      //
      if( hasSessionSynchronizationInterface(bean)
         && session.isBeanManagedTx() )
      {
         fireSpecViolationEvent(session, new Section("7.6.1"));
         status = false;
      }

      // The session bean class MUST be defined as public.
      //
      // Spec 7.10.2
      //
      if (!isPublic(bean))
      {
         fireSpecViolationEvent(session, new Section("7.10.2.b1"));
         status = false;
      }

      // The session bean class MUST NOT be final.
      //
      // Spec 7.10.2
      //
      if (isFinal(bean))
      {
         fireSpecViolationEvent(session, new Section("7.10.2.b2"));
         status = false;
      }

      // The session bean class MUST NOT be abstract.
      //
      // Spec 7.10.2
      //
      if (isAbstract(bean))
      {
         fireSpecViolationEvent(session, new Section("7.10.2.b3"));
         status = false;
      }

      // The session bean class MUST have a public constructor that
      // takes no arguments.
      //
      // Spec 7.10.2
      //
      if (!hasDefaultConstructor(bean))
      {
         fireSpecViolationEvent(session, new Section("7.10.2.c"));
         status = false;
      }

      // The session bean class MUST NOT define the finalize() method.
      //
      // Spec 7.10.2
      //
      if (hasFinalizer(bean))
      {
         fireSpecViolationEvent(session, new Section("7.10.2.d"));
         status = false;
      }

      // The ejbCreate(...) method signatures MUST follow these rules:
      //
      //      - The method name MUST have ejbCreate as its prefix
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as final or static
      //      - The return type MUST be void
      //      - The method arguments MUST be legal types for RMI/IIOP
      //      - The method SHOULD not throw a java.rmi.RemoteException
      //        (NOTE we don't test for this as it's not a MUST)
      //
      // Spec 7.10.3
      //
      if( hasEJBCreateMethod(bean, true) )
      {
         Iterator it = getEJBCreateMethods(bean);
         while (it.hasNext())
         {
            Method ejbCreate = (Method)it.next();

            if (!isPublic(ejbCreate))
            {
               fireSpecViolationEvent( session, ejbCreate,
                  new Section("7.10.3.b"));
               status = false;
            }

            if ( (isFinal(ejbCreate)) || (isStatic(ejbCreate)) )
            {
               fireSpecViolationEvent( session, ejbCreate,
                  new Section("7.10.3.c"));
               status = false;
            }

            if (!hasVoidReturnType(ejbCreate))
            {
               fireSpecViolationEvent( session, ejbCreate,
                  new Section("7.10.3.d"));
               status = false;
            }

            if (!hasLegalRMIIIOPArguments(ejbCreate))
            {
               fireSpecViolationEvent( session, ejbCreate,
                  new Section("7.10.3.e"));
               status = false;
            }
         }
      }

      return status;
   }

   /*
    * Verify Entity Bean Home Interface
    */
   private boolean verifyEntityHome(EntityMetaData entity)
   {
      boolean status = true;

      // Entity bean's home interface MUST extend the javax.ejb.EJBHome
      // interface.
      //
      // Spec 12.2.9
      //
      if (!hasEJBHomeInterface(home))
      {
         fireSpecViolationEvent(entity, new Section("12.2.9.a"));
         status = false;
      }

      // The methods defined in the entity bean's home interface MUST
      // have valid RMI-IIOP argument types.
      //
      // The methods defined in the entity bean's home interface MUST
      // have valid RMI-IIOP return types.
      //
      // The methods defined in the entity bean's home interface MUST
      // have java.rmi.RemoteException in their throws clause.
      //
      // Spec 12.2.9
      //
      Iterator methods = Arrays.asList(home.getMethods()).iterator();
      while (methods.hasNext())
      {
         Method method = (Method)methods.next();

         if (!hasLegalRMIIIOPArguments(method))
         {
            fireSpecViolationEvent( entity, method,
               new Section("12.2.9.b1") );
            status = false;
         }

         if (!hasLegalRMIIIOPReturnType(method))
         {
            fireSpecViolationEvent( entity, method,
               new Section("12.2.9.b2") );
            status = false;
         }

         if (!throwsRemoteException(method))
         {
            fireSpecViolationEvent( entity, method,
               new Section("12.2.9.b3") );
            status = false;
         }
      }

      // Each method defined in the entity bean's home interface must be
      // one of the following:
      //
      //    - a create method
      //    - a finder method
      //    - a home method
      //
      // Spec 12.2.9
      //
      methods = Arrays.asList(home.getMethods()).iterator();
      while (methods.hasNext())
      {
         Method method = (Method)methods.next();

         // Do not check the methods of the javax.ejb.EJBHome interface
         if (method.getDeclaringClass().getName().equals(EJB_HOME_INTERFACE))
            continue;

         if (isCreateMethod(method))
         {
            // Each create(...) method in the entity bean's home
            // interface MUST have a matching ejbCreate(...) method in
            // the entity bean's class.
            //
            // Each create(...) method in the entity bean's home
            // interface MUST have the same number and types of
            // arguments to its matching ejbCreate(...) method.
            //
            // The return type for a create(...) method MUST be the
            // entity bean's remote interface type.
            //
            // All the exceptions defined in the throws clause of the
            // matching ejbCreate(...) and ejbPostCreate(...) methods of
            // the enterprise bean class MUST be included in the throws
            // clause of a matching create(...) method.
            //
            // The throws clause of a create(...) method MUST include
            // the javax.ejb.CreateException.
            //
            // Spec 12.2.9
            //
            if (!hasMatchingEJBCreate(bean, method))
            {
               fireSpecViolationEvent(entity, method, new Section("12.2.9.d"));
               status = false;
            }

            if (!hasRemoteReturnType(entity, method))
            {
               fireSpecViolationEvent(entity, method, new Section("12.2.9.e"));
               status = false;
            }

            if( hasMatchingEJBCreate(bean, method)
               && hasMatchingEJBPostCreate(bean, method) )
            {
               Method ejbCreate = getMatchingEJBCreate(bean, method);
               Method ejbPostCreate = getMatchingEJBPostCreate(bean, method);

               if ( !(hasMatchingExceptions(ejbCreate, method)
                  && hasMatchingExceptions(ejbPostCreate, method)) )
               {
                  fireSpecViolationEvent(entity, method,
                     new Section("12.2.9.f"));
               }
            }

            if (!throwsCreateException(method))
            {
               fireSpecViolationEvent(entity, method, new Section("12.2.9.g"));
               status = false;
            }
         }
         else if (isFinderMethod(method))
         {
            // Each finder method MUST match one of the ejbFind<METHOD>
            // methods defined in the entity bean class.
            //
            // The matching ejbFind<METHOD> method MUST have the same
            // number and types of arguments.
            //
            // The return type for a find<METHOD> method MUST be the
            // entity bean's remote interface type (single-object
            // finder) or a collection (java.util.Collection Spec 10.5.6.2)
            // thereof (for a multi-object finder).
            //
            // All the exceptions defined in the throws clause of an
            // ejbFind method of the entity bean class MUST be included
            // in the throws clause of the matching find method of the
            // home interface.
            //
            // The throws clause of a finder method MUST include the
            // javax.ejb.FinderException.
            //
            // Spec 12.2.9
            //
            if( entity.isBMP() )
            {  // Check for BMP violations
               if ( (!hasMatchingEJBFind(bean, method)))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.9.h"));
                  status = false;
               }

               if ( !(hasRemoteReturnType(entity, method)
                  || isMultiObjectFinder(method)) )
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.9.j"));
                  status = false;
               }

               if ((hasMatchingEJBFind(bean, method)))
               {
                  Method ejbFind  = getMatchingEJBFind(bean, method);
                  if ( !(hasMatchingExceptions(ejbFind, method)))
                  {
                     fireSpecViolationEvent( entity, method,
                        new Section("12.2.9.k"));
                     status = false;
                  }
               }

               if (!throwsFinderException(method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.9.l"));
                  status = false;
               }
            } // if( entity.isBMP() )

            if( entity.isCMP() )
            {

               if ( !(hasRemoteReturnType(entity, method)
                  || isMultiObjectFinder(method)))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("10.6.10.a"));
                  status = false;
               }

               if (!throwsFinderException(method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("10.6.10.b"));
                  status = false;
               }

               // For every finder method there must be a matching
               // <query> element defined in the deployment descriptor
               // with the exception of findByPrimaryKey
               //
               // JBoss Extension: 'findAll' is _also_ ignored.
               //
               if( !method.getName().equals("findByPrimaryKey")
                  && !method.getName().equals("findAll")
                  && !hasMatchingQuery(method, entity) )
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("10.5.6") );
                  status = false;
               }
            } // if( entity.isCMP() )
         }
         else   // Neither Create nor Finder method
         {
            // Each home method MUST match a method defined in the
            // entity bean class.
            //
            // The matching ejbHome<METHOD> method MUST have the same
            // number and types of arguments, and a matching return
            // type.
            //
            // Spec 12.2.9
            //
            if (!hasMatchingEJBHome(bean, method))
            {
               fireSpecViolationEvent(entity, method,
                  new Section("12.2.9.m"));
               status = false;
            }
         }
      } // while( methods.hasNext() )


      return status;
   }

   /*
    * Verify Entity Bean Local Home Interface
    */
   private boolean verifyEntityLocalHome(EntityMetaData entity)
   {
      boolean status = true;

      // Entity bean's local home interface MUST extend the
      // javax.ejb.EJBLocalHome interface.
      //
      // Spec 12.2.11
      //
      if( !hasEJBLocalHomeInterface(localHome) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.11.a"));
         status = false;
      }

      // The methods defined in the entity bean's home interface MUST
      // NOT have java.rmi.RemoteException in their throws clause.
      //
      // Spec 12.2.11
      //
      Iterator homeMethods = Arrays.asList(localHome.getMethods()).iterator();
      while (homeMethods.hasNext())
      {
         Method method = (Method)homeMethods.next();

         if (throwsRemoteException(method))
         {
            fireSpecViolationEvent(entity, method, new Section("12.2.11.b"));
            status = false;
         }
      }

      // Each method defined in the entity bean's local home interface
      // must be one of the following:
      //
      //    - a create method
      //    - a finder method
      //    - a home method
      //
      // Spec 12.2.11
      //
      homeMethods = Arrays.asList(localHome.getMethods()).iterator();
      while (homeMethods.hasNext())
      {
         Method method = (Method)homeMethods.next();

         // Do not check the methods of the javax.ejb.EJBLocalHome interface
         if( method.getDeclaringClass().getName().equals(EJB_LOCAL_HOME_INTERFACE) )
               continue;

         if (isCreateMethod(method))
         {
            // Each create(...) method in the entity bean's local home
            // interface MUST have a matching ejbCreate(...) method in
            // the entity bean's class.
            //
            // Each create(...) method in the entity bean's local home
            // interface MUST have the same number and types of
            // arguments to its matching ejbCreate(...) method.
            //
            // The return type for a create(...) method MUST be the
            // entity bean's local interface type.
            //
            // All the exceptions defined in the throws clause of the
            // matching ejbCreate(...) and ejbPostCreate(...) methods of
            // the enterprise bean class MUST be included in the throws
            // clause of a matching create(...) method.
            //
            // The throws clause of a create(...) method MUST include
            // the javax.ejb.CreateException.
            //
            // Spec 12.2.11
            //
            if (!hasMatchingEJBCreate(bean, method))
            {
               fireSpecViolationEvent(entity, method,
                  new Section("12.2.11.e"));
               status = false;
            }

            if (!hasLocalReturnType(entity, method))
            {
               fireSpecViolationEvent(entity, method,
                  new Section("12.2.11.f"));
               status = false;
            }

            if( hasMatchingEJBCreate(bean, method)
               && hasMatchingEJBPostCreate(bean, method) )
            {
               Method ejbCreate = getMatchingEJBCreate(bean, method);
               Method ejbPostCreate = getMatchingEJBPostCreate(bean, method);

               if( !(hasMatchingExceptions(ejbCreate, method)
                  && hasMatchingExceptions(ejbPostCreate, method)) )
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.11.g"));
               }
            }

            if (!throwsCreateException(method))
            {
               fireSpecViolationEvent(entity, method,
                  new Section("12.2.11.h"));
               status = false;
            }
         }
         else if (isFinderMethod(method))
         {
            // Each finder method MUST match one of the ejbFind<METHOD>
            // methods defined in the entity bean class.
            //
            // The matching ejbFind<METHOD> method MUST have the same
            // number and types of arguments.
            //
            // The return type for a find<METHOD> method MUST be the
            // entity bean's local interface type (single-object finder)
            // or a collection (java.util.Collection Spec 10.5.6.2)
            // thereof (for a multi-object finder).
            //
            // All the exceptions defined in the throws clause of an
            // ejbFind method of the entity bean class MUST be included
            // in the throws clause of the matching find method of the
            // home interface.
            //
            // The throws clause of a finder method MUST include the
            // javax.ejb.FinderException.
            //
            // Spec 12.2.11
            //
            if( !(hasLocalReturnType(entity, method)
               || isMultiObjectFinder(method)) )
            {
               fireSpecViolationEvent( entity, method,
                  new Section("12.2.11.j"));
               status = false;
            }

            if (!throwsFinderException(method))
            {
               fireSpecViolationEvent( entity, method,
                  new Section("12.2.11.k"));
               status = false;
            }

            if(entity.isCMP() )
            {
               // The entity bean class does not implement the finder
               // methods. The implementation of the finder methods are
               // provided by the Container
               //
               // Spec 10.6.2
               //
               if( hasMatchingEJBFind(bean, method) )
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("10.6.2.j"));
                  status = false;
               }

               // For every finder method there must be a matching
               // <query> element defined in the deployment descriptor
               // with the exception of findByPrimaryKey
               //
               // JBoss Extension: 'findAll' is _also_ ignored.
               //
               // Spec 10.5.6
               //
               if( !method.getName().equals("findByPrimaryKey")
                  && !method.getName().equals("findAll")
                  && !hasMatchingQuery(method, entity) )
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("10.5.6") );
                  status = false;
               }
            }

            if(entity.isBMP() )
            {
               if (!hasMatchingEJBFind(bean, method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.11.i"));
                  status = false;
               }
               else
               {
                  Method ejbFind  = getMatchingEJBFind(bean, method);

                  if ( !(hasMatchingExceptions(ejbFind, method)))
                  {
                     fireSpecViolationEvent( entity, method,
                        new Section("12.2.11.l"));
                  }
               }
            }
         }
         else
         {
            // Each home method MUST match a method defined in the
            // entity bean class.
            //
            // The matching ejbHome<METHOD> method MUST have the same
            // number and types of arguments, and a matching return
            // type.
            //
            // Spec 12.2.9
            //
            if (!hasMatchingEJBHome(bean, method))
            {
               fireSpecViolationEvent( entity, method,
                  new Section("12.2.11.m"));
               status = false;
            }
         }
      } // while( homeMethods.hasNext() )

      return status;
   }

   /*
    * Verify Entity Bean Local Interface
    */
   private boolean verifyEntityLocal(EntityMetaData entity)
   {
      boolean status = true;

      // Entity bean's local interface MUST extend
      // the javax.ejb.EJBLocalObject interface.
      //
      // Spec 12.2.10
      //
      if( !hasEJBLocalObjectInterface(local) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.10.a"));
         status = false;
      }

      // The methods defined in the entity bean's local interface MUST
      // NOT have java.rmi.RemoteException in their throws clause.
      //
      // Spec 12.2.10
      //
      Iterator localMethods = Arrays.asList(local.getMethods()).iterator();
      while (localMethods.hasNext())
      {
         Method method = (Method)localMethods.next();

         if (throwsRemoteException(method))
         {
            fireSpecViolationEvent(entity, method, new Section("12.2.10.b"));
            status = false;
         }
      }

      // For each method defined in the local interface, there MUST be
      // a matching method in the entity bean's class. The matching
      // method MUST have:
      //
      //     - The same name.
      //     - The same number and types of its arguments.
      //     - The same return type.
      //     - All the exceptions defined in the throws clause of the
      //       matching method of the enterprise Bean class must be
      //       defined in the throws clause of the method of the local
      //       interface.
      //
      // Spec 12.2.10
      //
      localMethods = Arrays.asList(local.getMethods()).iterator();
      while (localMethods.hasNext())
      {
         Method method = (Method)localMethods.next();

         // Do not check the methods of the javax.ejb.EJBLocalObject
         // interface
         if(method.getDeclaringClass().getName().equals(EJB_LOCAL_OBJECT_INTERFACE))
            continue;

         if (!hasMatchingMethod(bean, method))
         {
            fireSpecViolationEvent(entity, method, new Section("12.2.10.c"));
            status = false;
         }

         if (hasMatchingMethod(bean, method))
         {
            try
            {
               Method beanMethod = bean.getMethod( method.getName(),
                  method.getParameterTypes() );

               if (!hasMatchingReturnType(beanMethod, method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.10.d"));
                  status = false;
               }

               if (!hasMatchingExceptions(beanMethod, method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("12.2.10.e"));

                  status = false;
               }
            }
            catch (NoSuchMethodException ignored)
            {}
         }
      }

      return status;
   }

   /*
    * Verify Entity Bean Remote Interface
    */
   private boolean verifyEntityRemote( EntityMetaData entity )
   {
      boolean status = true;

      // Entity bean's remote interface MUST extend
      // the javax.ejb.EJBObject interface.
      //
      // Spec 9.2.7
      //
      if (!hasEJBObjectInterface(remote))
      {
         fireSpecViolationEvent(entity, new Section("9.2.7.a"));
         status = false;
      }

      // The methods defined in the entity bean's remote interface MUST
      // have valid RMI-IIOP argument types.
      //
      // The methods defined in the entity bean's home interface MUST
      // have valid RMI-IIOP return types.
      //
      // The methods defined in the entity bean's home interface MUST
      // have java.rmi.RemoteException in their throws clause.
      //
      // Spec 9.2.7
      //
      Iterator it = Arrays.asList(remote.getMethods()).iterator();
      while (it.hasNext())
      {
         Method method = (Method)it.next();

         if (!hasLegalRMIIIOPArguments(method))
         {
            fireSpecViolationEvent(entity, method, new Section("9.2.7.b"));
            status = false;
         }

         if (!hasLegalRMIIIOPReturnType(method))
         {
            fireSpecViolationEvent(entity, method, new Section("9.2.7.c"));
            status = false;
         }

         if (!hasLegalRMIIIOPExceptionTypes(method))
         {
            fireSpecViolationEvent(entity, method, new Section("9.2.7.h"));
            status = false;
         }

         if (!throwsRemoteException(method))
         {
            fireSpecViolationEvent(entity, method, new Section("9.2.7.d"));
            status = false;
         }
      }

      // For each method defined in the remote interface, there MUST be
      // a matching method in the entity bean's class. The matching
      // method MUST have:
      //
      //     - The same name.
      //     - The same number and types of its arguments.
      //     - The same return type.
      //     - All the exceptions defined in the throws clause of the
      //       matching method of the enterprise Bean class must be
      //       defined in the throws clause of the method of the remote
      //       interface.
      //
      // Spec 9.2.7
      //
      it = Arrays.asList(remote.getMethods()).iterator();
      while (it.hasNext())
      {
         Method method = (Method)it.next();

         // Do not check the methods of the javax.ejb.EJBObject interface
         if (method.getDeclaringClass().getName().equals(EJB_OBJECT_INTERFACE))
            continue;

         if (!hasMatchingMethod(bean, method))
         {
            fireSpecViolationEvent(entity, method, new Section("9.2.7.e"));
            status = false;
         }

         if (hasMatchingMethod(bean, method))
         {
            try
            {
               Method beanMethod = bean.getMethod( method.getName(),
                  method.getParameterTypes());

               if (!hasMatchingReturnType(beanMethod, method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("9.2.7.f"));
                  status = false;
               }

               if (!hasMatchingExceptions(beanMethod, method))
               {
                  fireSpecViolationEvent( entity, method,
                     new Section("9.2.7.g"));
                  status = false;
               }
            }
            catch (NoSuchMethodException ignored)
            {}
         }
      }

      return status;
   }

   /*
    * Verify Entity Bean Class
    */
   private boolean verifyCMPEntityBean(EntityMetaData entity)
   {
      boolean status = true;

      // The enterprise bean class MUST implement, directly or
      // indirectly, the javax.ejb.EntityBean interface.
      //
      // Spec 10.6.2
      //
      if (!hasEntityBeanInterface(bean))
      {
         fireSpecViolationEvent(entity, new Section("10.6.2.a"));
         status = false;
      }

      // The entity bean class MUST be defined as public and abstract.
      //
      // Spec 10.6.2
      //
      if (!isPublic(bean) || !isAbstract(bean))
      {
         fireSpecViolationEvent(entity, new Section("10.6.2.b"));
         status = false;
      }

      // The entity bean class MUST define a public constructor that
      // takes no arguments
      //
      // Spec 10.6.2
      //
      if (!hasDefaultConstructor(bean))
      {
         fireSpecViolationEvent(entity, new Section("10.6.2.c"));
         status = false;
      }

      // The entity bean class MUST NOT define the finalize() method.
      //
      // Spec 10.6.2
      //
      if (hasFinalizer(bean))
      {
         fireSpecViolationEvent(entity, new Section("10.6.2.d"));
         status = false;
      }

      // The ejbCreate(...) method signatures MUST follow these rules:
      //
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as final or static
      //      - The return type MUST be the entity bean's primary key type
      //      --- Only if method is on remote home ---
      //      - The method arguments MUST be legal types for RMI/IIOP
      //      - The method return value type MUST be legal type for RMI/IIOP
      //      --- End of only if method is on remote home ---
      //      - The method must define the javax.ejb.CreateException
      //
      // Spec 10.6.4
      //
      if( hasEJBCreateMethod(bean, false) )
      {
         Iterator it = getEJBCreateMethods(bean);
         while( it.hasNext() )
         {
            Method ejbCreate = (Method)it.next();
            if (!isPublic(ejbCreate))
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("10.6.4.b") );
               status = false;
            }

            if ( (isFinal(ejbCreate)) || (isStatic(ejbCreate)) )
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("10.6.4.c"));
               status = false;
            }

            if (!hasPrimaryKeyReturnType(entity, ejbCreate))
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("10.6.4.d") );
               status = false;
            }

            /* FIXME
             *  This is only true if the method is on the remote home
             * interface
             if (!hasLegalRMIIIOPArguments(ejbCreate)) {
             fireSpecViolationEvent(entity, ejbCreate, new Section("10.6.4.d"));
             status = false;
             }

             if (!hasLegalRMIIIOPReturnType(ejbCreate)) {
             fireSpecViolationEvent(entity, ejbCreate, new Section("10.5.4.f"));
             status = false;
             }
             */

            if (!throwsCreateException(ejbCreate))
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("10.6.4.g"));
               status = false;
            }
         }
      }

      // For each ejbCreate(...) method, the entity bean class MUST
      // define a matching ejbPostCreate(...) method.
      //
      // The ejbPostCreate(...) method MUST follow these rules:
      //
      //   - the method MUST be declared as public
      //   - the method MUST NOT be declared as final or static
      //   - the return type MUST be void
      //   - the method arguments MUST be the same as the matching
      //     ejbCreate(...) method
      //
      // Spec 10.6.5
      //
      if (hasEJBCreateMethod(bean, false))
      {
         Iterator it = getEJBCreateMethods(bean);

         while( it.hasNext() )
         {
            Method ejbCreate = (Method)it.next();

            if (!hasMatchingEJBPostCreate(bean, ejbCreate))
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("10.6.5.a") );
               status = false;
            }

            if( hasMatchingEJBPostCreate(bean, ejbCreate) )
            {
               Method ejbPostCreate = getMatchingEJBPostCreate( bean,
                  ejbCreate );

               if (!isPublic(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("10.6.5.b"));
                  status = false;
               }

               if (isStatic(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("10.6.5.c"));
                  status = false;
               }

               if (isFinal(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("10.6.5.d"));
                  status = false;
               }

               if(!hasVoidReturnType(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("10.6.5.e"));
                  status = false;
               }
            }
         }
      }

      // The ejbHome(...) method signatures MUST follow these rules:
      //
      //      - The method name MUST have ejbHome as its prefix.
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as static.
      //      - The method MUST NOT define the java.rmi.RemoteException
      //
      // Spec 10.6.6
      //
      Iterator it = getEjbHomeMethods(bean);
      while (it.hasNext())
      {
         Method ejbHome = (Method)it.next();
         if (!isPublic(ejbHome))
         {
            fireSpecViolationEvent(entity, ejbHome, new Section("10.6.6.a"));
            status = false;
         }

         if (isStatic(ejbHome))
         {
            fireSpecViolationEvent(entity, ejbHome, new Section("10.6.6.b"));
            status = false;
         }

         if (throwsRemoteException(ejbHome))
         {
            fireSpecViolationEvent(entity, ejbHome, new Section("10.6.6.c"));
            status = false;
         }
      }

      // The CMP entity bean MUST implement get and set accessor methods for
      // each field within the abstract persistance schema.
      //
      // Spec 10.6.2
      //
      it = entity.getCMPFields();
      while(it.hasNext())
      {
         String fieldName = (String)it.next();
         String getName = "get" + fieldName.substring(0,1).toUpperCase() +
         fieldName.substring(1);
         Class fieldType = null;

         try
         {
            Method m = bean.getMethod(getName, new Class[0]);
            fieldType = m.getReturnType();

            // The getter must not return 'void' according to the JavaBeans
            // Spec
            if( fieldType == Void.TYPE )
            {
               fireSpecViolationEvent( entity,
                  new Section("jb.7.1.b", "Field: " + fieldName) );
            }
         }
         catch (NoSuchMethodException nsme)
         {
            fireSpecViolationEvent( entity,
               new Section("10.6.2.g", "Field: " + fieldName) );
            status = false;
         }

         String setName = "set" + fieldName.substring(0,1).toUpperCase() +
         fieldName.substring(1);
         Class[] args = new Class[1];
         args[0] = fieldType;

         try
         {
            Method m = bean.getMethod(setName, args);
            fieldType = m.getReturnType();

            // According to the JavaBeans Spec, a setter method must
            // return 'void'
            if( fieldType != Void.TYPE )
            {
               fireSpecViolationEvent( entity,
                  new Section("jb.7.1.a", "Field: " + fieldName) );
            }
         }
         catch (NoSuchMethodException nsme)
         {
            // Try with java.util.Collection
            //
            // FIXME: This should only be tried for CMR methods; a CMP
            //        setter cannot accept a Collection!
            try
            {
               args[0] = classloader.loadClass("java.util.Collection");
               Method m = bean.getMethod(setName, args);
            }
            catch( NoSuchMethodException nsme2 )
            {
               fireSpecViolationEvent(entity,
                  new Section("10.6.2.h", "Field: " + fieldName ));
               status = false;
            }
            catch( ClassNotFoundException cnfe )
            {
               // Something is really broken
            }
         }
      }

      // The ejbSelect(...) method signatures MUST follow these rules:
      //
      //      - The method name MUST have ejbSelect as its prefix.
      //      - The method MUST be declared as public
      //      - The method MUST be declared as abstract.
      //      - The method MUST define the javax.ejb.FinderException
      //
      // Spec 10.6.7
      //
      it = getEjbSelectMethods(bean);
      while( it.hasNext() )
      {
         Method ejbSelect = (Method)it.next();

         if (!isPublic(ejbSelect))
         {
            fireSpecViolationEvent(entity, ejbSelect, new Section("10.6.7.a"));
            status = false;
         }

         if (!isAbstract(ejbSelect))
         {
            fireSpecViolationEvent(entity, ejbSelect, new Section("10.6.7.b"));
            status = false;
         }

         if (!throwsFinderException(ejbSelect))
         {
            fireSpecViolationEvent(entity, ejbSelect, new Section("10.6.7.c"));
            status = false;
         }

         if( !hasMatchingQuery(ejbSelect, entity) )
         {
            fireSpecViolationEvent( entity, ejbSelect, new Section("10.5.7") );
            status = false;
         }
      }

      // A CMP Entity Bean must not define Finder methods.
      //
      // Spec 10.6.2
      //
      if( hasFinderMethod(bean) )
      {
         fireSpecViolationEvent(entity, new Section("10.6.2.i"));
         status = false;
      }

      return status;
   }

   /*
    * Verify BMP Entity Class
    */
   private boolean verifyBMPEntityBean(EntityMetaData entity)
   {
      boolean status = true;

      // The enterprise bean class MUST implement, directly or
      // indirectly, the javax.ejb.EntityBean interface.
      //
      // Spec 12.2.2
      //
      if( !hasEntityBeanInterface(bean) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.2.a"));
         status = false;
      }

      // The entity bean class MUST be defined as public and NOT abstract.
      //
      // Spec 12.2.2
      //
      if( !isPublic(bean) || isAbstract(bean) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.2.b"));
         status = false;
      }

      // The entity bean class MUST NOT be defined as final.
      //
      // Spec 12.2.2
      //
      if( isFinal(bean) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.2.c"));
         status = false;
      }

      // The entity bean class MUST define a public constructor that
      // takes no arguments
      //
      // Spec 12.2.2
      //
      if( !hasDefaultConstructor(bean) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.2.d"));
         status = false;
      }

      // The entity bean class MUST NOT define the finalize() method.
      //
      // Spec 12.2.2
      //
      if( hasFinalizer(bean) )
      {
         fireSpecViolationEvent(entity, new Section("12.2.2.e"));
         status = false;
      }

      // The ejbCreate(...) method signatures MUST follow these rules:
      //
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as final or static
      //      - The return type MUST be the entity bean's primary key type
      //      --- If the method is on the remote home interface ---
      //      - The method arguments MUST be legal types for RMI/IIOP
      //      - The method return value type MUST be legal type for RMI/IIOP
      //      --- End if the method is on the remote home interface ---
      //
      // Spec 12.2.3
      //
      if( hasEJBCreateMethod(bean, false) )
      {
         Iterator it = getEJBCreateMethods(bean);
         while (it.hasNext())
         {
            Method ejbCreate = (Method)it.next();
            if (!isPublic(ejbCreate))
            {
               fireSpecViolationEvent(entity, ejbCreate,
                  new Section("12.2.3.a") );
               status = false;
            }

            if ( (isFinal(ejbCreate)) || (isStatic(ejbCreate)) )
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("12.2.3.b"));
               status = false;
            }

            if (!hasPrimaryKeyReturnType(entity, ejbCreate))
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("12.2.3.c"));
               status = false;
            }

            /* FIXME
             * This code needs to only be invoked if the method is on the
             * remote home.
             if (!hasLegalRMIIIOPArguments(ejbCreate)) {
             fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.d"));
             status = false;
             }
             if (!hasLegalRMIIIOPReturnType(ejbCreate)) {
             fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.e"));
             status = false;
             }
            */
         }
      }

      // For each ejbCreate(...) method, the entity bean class MUST
      // define a matching ejbPostCreate(...) method.
      //
      // The ejbPostCreate(...) method MUST follow these rules:
      //
      //   - the method MUST be declared as public
      //   - the method MUST NOT be declared as final or static
      //   - the return type MUST be void
      //   - the method arguments MUST be the same as the matching
      //     ejbCreate(...) method
      //
      // Spec 12.2.4
      //
      if( hasEJBCreateMethod(bean, false) )
      {
         Iterator it =  getEJBCreateMethods(bean);
         while (it.hasNext())
         {
            Method ejbCreate = (Method)it.next();

            if (!hasMatchingEJBPostCreate(bean, ejbCreate))
            {
               fireSpecViolationEvent( entity, ejbCreate,
                  new Section("12.2.4.a"));
               status = false;
            }

            if (hasMatchingEJBPostCreate(bean, ejbCreate))
            {
               Method ejbPostCreate = getMatchingEJBPostCreate( bean,
                  ejbCreate);

               if (!isPublic(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("12.2.4.b"));
                  status = false;
               }

               if (isStatic(ejbPostCreate) || isFinal(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("12.2.4.c"));
                  status = false;
               }

               if(!hasVoidReturnType(ejbPostCreate))
               {
                  fireSpecViolationEvent( entity, ejbPostCreate,
                     new Section("12.2.4.d"));
                  status = false;
               }
            }
         }
      }

      // Every entity bean MUST define the ejbFindByPrimaryKey method.
      //
      // The return type for the ejbFindByPrimaryKey method MUST be the
      // primary key type.
      //
      // The ejbFindByPrimaryKey method MUST be a single-object finder.
      //
      // Spec 12.2.5
      //
      if (!hasEJBFindByPrimaryKey(bean))
      {
         fireSpecViolationEvent(entity, new Section("12.2.5.e"));
         status = false;
      }

      if (hasEJBFindByPrimaryKey(bean))
      {
         Method ejbFindByPrimaryKey = getEJBFindByPrimaryKey(bean);

         if (!hasPrimaryKeyReturnType(entity, ejbFindByPrimaryKey))
         {
            fireSpecViolationEvent( entity, ejbFindByPrimaryKey,
               new Section("12.2.5.e1"));
            status = false;
         }

         if (!isSingleObjectFinder(entity, ejbFindByPrimaryKey))
         {
            fireSpecViolationEvent( entity, ejbFindByPrimaryKey,
               new Section("12.2.5.e2"));
            status = false;
         }
      }

      // A finder method MUST be declared as public.
      //
      // A finder method MUST NOT be declared as static.
      //
      // A finder method MUST NOT be declared as final.
      //
      // The finder method argument types MUST be legal types for
      // RMI/IIOP
      //
      // The finder method return type MUST be either the entity bean's
      // primary key type, or java.lang.util.Enumeration interface or
      // java.lang.util.Collection interface.
      //
      // Spec 12.2.5
      //
      if (hasFinderMethod(bean))
      {
         Iterator it = getEJBFindMethods(bean);
         while (it.hasNext())
         {
            Method finder = (Method)it.next();

            if (!isPublic(finder))
            {
               fireSpecViolationEvent(entity, finder, new Section("12.2.5.a"));
               status = false;
            }

            if (isFinal(finder) || isStatic(finder))
            {
               fireSpecViolationEvent(entity, finder, new Section("12.2.5.b"));
               status = false;
            }

            /** FIXME
             * this path should only get invoked if the finder is on the
             * remote interface.
             if (!hasLegalRMIIIOPArguments(finder)) {
               fireSpecViolationEvent(entity, finder, new Section("12.2.5.c"));
               status = false;
             }
             */

            if ( !(isSingleObjectFinder(entity, finder)
               || isMultiObjectFinder(finder)))
            {
               fireSpecViolationEvent(entity, finder, new Section("12.2.5.d"));
               status = false;
            }
         }
      }

      // The ejbHome(...) method signatures MUST follow these rules:
      //
      //      - The method name MUST have ejbHome as its prefix.
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as static.
      //      - The method MUST NOT define the java.rmi.RemoteException
      //
      // Spec 10.6.6
      //
      Iterator it = getEjbHomeMethods(bean);
      while (it.hasNext())
      {
         Method ejbHome = (Method)it.next();

         if (!isPublic(ejbHome))
         {
            fireSpecViolationEvent(entity, ejbHome, new Section("10.6.6.a"));
            status = false;
         }

         if (isStatic(ejbHome))
         {
            fireSpecViolationEvent(entity, ejbHome, new Section("10.6.6.b"));
            status = false;
         }

         if (throwsRemoteException(ejbHome))
         {
            fireSpecViolationEvent(entity, ejbHome, new Section("10.6.6.c"));
            status = false;
         }
      }

      return status;
   }

   /*
    * Verify Primary Key
    */
   private boolean verifyPrimaryKey(EntityMetaData entity)
   {
      boolean status = true;
      boolean cmp = entity.isCMP();

      if( entity.getPrimaryKeyClass() == null
         || entity.getPrimaryKeyClass().length() == 0 )
      {
         if( cmp )
            fireSpecViolationEvent(entity, new Section("10.6.1.a"));
         else
            fireSpecViolationEvent(entity, new Section("12.2.1.a"));

         // We can't get any further if there's no PK class specified!
         return false; 
      }

      // FIXME - Still missing the bits from 10.8.2 for CMP primary
      // keys.  Primarily the class must be public, all fields in the
      // class must be public and the fields must also be a subset of
      // the CMP fields within the bean.
      //
      Class cls = null;
      try
      {
         cls = classloader.loadClass(entity.getPrimaryKeyClass());
      } catch(ClassNotFoundException e)
      {
         if( cmp )
            fireSpecViolationEvent(entity, new Section("10.6.13.a"));
         else
            fireSpecViolationEvent(entity, new Section("12.2.12.a"));

         // Can't do any other checks if the class is null!
         return false;
      }

      // The primary key type must be a valid type in RMI-IIOP.
      //
      // Spec 10.6.13 & 12.2.12
      //
      if( !isRMIIDLValueType(cls) )
      {
         if( cmp )
            fireSpecViolationEvent(entity, new Section("10.6.13.b"));
         else
            fireSpecViolationEvent(entity, new Section("12.2.12.b"));
         status = false;
      }

      // No primary key field specified, just a primary key class.
      if( entity.getPrimKeyField() == null ||
         entity.getPrimKeyField().length() == 0 )
      {
         // This is a check for some interesting implementation of
         // equals() and hashCode().  I am not sure how well it works in
         // the end.
         //
         if( !cls.getName().equals("java.lang.Object") )
         {
            Object one, two;

            try
            {
               one = cls.newInstance();
               two = cls.newInstance();
               try
               {
                  if( !one.equals(two) )
                  {
                     if( cmp )
                     {
                        // fireSpecViolationEvent(entity, new Section("10.6.13.c"));
                        log.warn("Default instances of primary key: "+cls
                           + " do not equate, check your equals method");
                     }
                     else
                     {
                        //fireSpecViolationEvent(entity, new Section("12.2.12.c"));
                        log.warn("Default instances of primary key: "+cls
                           + " do not equate, check your equals method");
                     }
                     status = true;
                  }
               }
               catch(NullPointerException e)
               {
                  // That's OK - the implementor expected the fields to
                  // have values
               }

               try
               {
                  if(one.hashCode() != two.hashCode())
                  {
                     if( cmp )
                     {
                        //fireSpecViolationEvent(entity, new Section("10.6.13.d"));
                        log.warn("Default instances of primary key: "+cls
                           + " do not have the same hash, check your hashCode method");
                     }
                     else
                     {
                        //fireSpecViolationEvent(entity, new Section("12.2.12.d"));
                        log.warn("Default instances of primary key: "+cls
                           + " do not have the same hash, check your hashCode method");
                     }
                     status = true;
                  }
               } catch(NullPointerException e)
               {
                  // That's OK - the implementor expected the fields to have values
               }
            }
            catch(IllegalAccessException e)
            {
               // If CMP primary key class MUST have a public
               // constructor with no parameters.  10.8.2.a
               ///
               if( cmp )
               {
                  fireSpecViolationEvent(entity, new Section("10.8.2.a"));
                  status = false;
               }
            }
            catch(InstantiationException e)
            {
               //Not sure what condition this is at the moment - JMW
               //fireSpecViolationEvent(entity, new Section("9.2.9.a"));
               //status = false;
            }
         }
      }
      else
      {
         //  BMP Beans MUST not include the primkey-field element in
         //  their deployment descriptor.  Deployment descriptor comment
         //
         if( entity.isBMP() )
         {
            fireSpecViolationEvent(entity, new Section("dd.a"));
            status = false;
         }

         // The primary keyfield MUST be a CMP field within the
         // entity bean.
         //
         // Spec 10.8.1
         //
         boolean found = false;
         Iterator it = entity.getCMPFields();
         while( it.hasNext() )
         {
            String fieldName = (String)it.next();
            if( fieldName.equals(entity.getPrimKeyField()) )
            {
               found = true;
               break;
            }
         }

         if( !found )
         {
            status = false;
            fireSpecViolationEvent(entity, new Section("10.8.1.b"));
         }

         try
         {
            // The class of the primary key field MUST match the
            // primary key class specified for the entity bean.  We
            // figure out the class of this field by getting the
            // return type of the get<FieldName> accessor method.
            //
            // Spec 10.8.1
            //
            String pkField = entity.getPrimKeyField();
            String methodName = "get" +
               pkField.substring(0,1).toUpperCase() + pkField.substring(1);

            Method method = bean.getMethod(methodName, new Class[0]);
            if( !entity.getPrimaryKeyClass().equals(
               method.getReturnType().getName())
               )
            {
               status = false;
               fireSpecViolationEvent(entity, new Section("10.8.1.a"));
            }

         } catch(NoSuchMethodException e)
         {
            // The primary keyfield MUST be a CMP field within the
            // entity bean.
            //
            // Spec 10.8.1
            //
            status = false;
            fireSpecViolationEvent(entity, new Section("10.8.1.b"));
         }
      }

      return status;
   }

   /*
    * Verify Message Driven Bean
    */
   private boolean verifyMessageDrivenBean( MessageDrivenMetaData mdBean )
   {
      boolean status = true;

      // A message driven bean MUST implement, directly or indirectly,
      // javax.ejb.MessageDrivenBean interface.
      //
      // Spec 15.7.2
      //
      if (!hasMessageDrivenBeanInterface(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.a"));
         status = false;
      }

      // A message driven bean MUST implement, directly or indirectly,
      // javax.jms.MessageListener interface.
      //
      // Spec 15.7.2
      //
      if (!hasMessageListenerInterface(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.b"));
         status = false;
      }

      // The message driven bean class MUST be defined as public.
      //
      // Spec 15.7.2
      //
      if (!isPublic(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.c1"));
         status = false;
      }

      // The message driven bean class MUST NOT be final.
      //
      // Spec 15.7.2
      //
      if (isFinal(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.c2"));
         status = false;
      }

      // The message driven bean class MUST NOT be abstract.
      //
      // Spec 15.7.2
      //
      if (isAbstract(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.c3"));
         status = false;
      }

      // The message driven bean class MUST have a public constructor that
      // takes no arguments.
      //
      // Spec 15.7.2
      //
      if (!hasDefaultConstructor(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.d"));
         status = false;
      }

      // The message driven bean class MUST NOT define the finalize() method.
      //
      // Spec 15.7.2
      //
      if (hasFinalizer(bean))
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.2.e"));
         status = false;
      }

      // A message driven bean MUST implement the ejbCreate() method.
      // The ejbCreate() method signature MUST follow these rules:
      //
      //      - The method name MUST be ejbCreate
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as final or static
      //      - The return type MUST be void
      //      - The method arguments MUST have no arguments.
      //      - The method MUST NOT define any application exceptions.
      //
      // Spec 15.7.2, 3
      //
      if (hasEJBCreateMethod(bean, false))
      {
         Iterator it = getEJBCreateMethods(bean);
         Method ejbCreate = (Method)it.next();

         if (!isPublic(ejbCreate))
         {
            fireSpecViolationEvent(mdBean, ejbCreate, new Section("15.7.3.b"));
            status = false;
         }

         if ( (isFinal(ejbCreate)) || (isStatic(ejbCreate)) )
         {
            fireSpecViolationEvent(mdBean, ejbCreate, new Section("15.7.3.c"));
            status = false;
         }

         if (!hasVoidReturnType(ejbCreate))
         {
            fireSpecViolationEvent(mdBean, ejbCreate, new Section("15.7.3.d"));
            status = false;
         }

         if (!hasNoArguments(ejbCreate))
         {
            fireSpecViolationEvent(mdBean, ejbCreate, new Section("15.7.3.e"));
            status = false;
         }

         if (!throwsNoException(ejbCreate))
         {
            fireSpecViolationEvent(mdBean, ejbCreate, new Section("15.7.3.f"));
            status = false;
         }

         if (it.hasNext())
         {
            fireSpecViolationEvent(mdBean, new Section("15.7.3.a"));
            status = false;
         }
      }
      else
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.3.a"));
         status = false;
      }

      // A message driven bean MUST implement the onMessage(...) method.
      // The onMessage() method signature MUST follow these rules:
      //
      //      - The method name MUST be onMessage
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as final or static
      //      - The return type MUST be void
      //      - The method arguments MUST have a single argument of type
      //        javax.jms.Message.
      //      - The method MUST NOT define any application exceptions.
      //
      // Spec 15.7.4
      //
      if( hasOnMessageMethod(bean) )
      {
         Iterator it = getOnMessageMethods(bean);
         Method onMessage = (Method)it.next();

         if (!isPublic(onMessage))
         {
            fireSpecViolationEvent(mdBean, onMessage, new Section("15.7.4.b"));
            status = false;
         }

         if ( (isFinal(onMessage)) || (isStatic(onMessage)) )
         {
            fireSpecViolationEvent(mdBean, onMessage, new Section("15.7.4.c"));
            status = false;
         }

         try
         {
            Class message = classloader.loadClass("javax.jms.Message");
            if (!hasSingleArgument(onMessage, message))
            {
               fireSpecViolationEvent( mdBean, onMessage,
                  new Section("15.7.4.e"));
               status = false;
            }

            if( !throwsNoException(onMessage) )
            {
               fireSpecViolationEvent( mdBean, onMessage,
                  new Section("15.7.4.f"));
               status = false;
            }

            if (it.hasNext())
            {
               fireSpecViolationEvent(mdBean, new Section("15.7.4.a"));
               status = false;
            }
         }
         catch( ClassNotFoundException cnfe )
         {
            // javax.jms.Message is not available?!
         }
      }
      else
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.4.a"));
         status = false;
      }

      // A message driven bean MUST implement the ejbRemove() method.
      // The ejbRemove() method signature MUST follow these rules:
      //
      //      - The method name MUST be ejbRemove
      //      - The method MUST be declared as public
      //      - The method MUST NOT be declared as final or static
      //      - The return type MUST be void
      //      - The method MUST have no arguments.
      //      - The method MUST NOT define any application exceptions.
      //
      // Spec 15.7.5
      //
      if (hasEJBRemoveMethod(bean))
      {
         Iterator it = getEJBRemoveMethods(bean);
         Method ejbRemove = (Method)it.next();

         if (!isPublic(ejbRemove))
         {
            fireSpecViolationEvent(mdBean, ejbRemove, new Section("15.7.5.b"));
            status = false;
         }

         if ( (isFinal(ejbRemove)) || (isStatic(ejbRemove)) )
         {
            fireSpecViolationEvent(mdBean, ejbRemove, new Section("15.7.5.c"));
            status = false;
         }

         if (!hasVoidReturnType(ejbRemove))
         {
            fireSpecViolationEvent(mdBean, ejbRemove, new Section("15.7.5.d"));
            status = false;
         }

         if (!hasNoArguments(ejbRemove))
         {
            fireSpecViolationEvent(mdBean, ejbRemove, new Section("15.7.5.e"));
            status = false;
         }

         if (!throwsNoException(ejbRemove))
         {
            fireSpecViolationEvent(mdBean, ejbRemove, new Section("15.7.5.f"));
            status = false;
         }

         if (it.hasNext())
         {
            fireSpecViolationEvent(mdBean, new Section("15.7.5.a"));
            status = false;
         }
      }
      else
      {
         fireSpecViolationEvent(mdBean, new Section("15.7.5.a"));
         status = false;
      }

      return status;
   }

}
/*
vim:ts=3:sw=3:et
*/
