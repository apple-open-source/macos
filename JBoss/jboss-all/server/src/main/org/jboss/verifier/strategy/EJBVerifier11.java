package org.jboss.verifier.strategy;

/*
 * Class org.jboss.verifier.strategy.EJBVerifier11
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
 * $Id: EJBVerifier11.java,v 1.30 2002/06/04 15:18:10 lqd Exp $
 */


// standard imports
import java.util.Iterator;
import java.util.Arrays;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Field;

// non-standard class dependencies
import org.jboss.verifier.Section;
import org.jboss.verifier.factory.DefaultEventFactory;

import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.SessionMetaData;
import org.jboss.metadata.EntityMetaData;


/**
 * Concrete implementation of the <code>VerificationStrategy</code> interface.
 * This class implements the verification of both session and entity beans for
 * Enterprise JavaBeans v1.1 specification.
 *
 * For more detailed documentation, refer to the
 * <a href="http://java.sun.com/products/ejb/docs.html">Enterprise JavaBeans v1.1, Final Release</a>
 *
 * @see     org.jboss.verifier.strategy.AbstractVerifier
 *
 * @author  <a href="mailto:juha.lindfors@jboss.org">Juha Lindfors</a>
 * @author  Aaron Mulder  (ammulder@alumni.princeton.edu)
 * @author  Vinay Menon   (menonv@cpw.co.uk)
 *
 * @version $Revision: 1.30 $
 * @since   JDK 1.3
 */
public class EJBVerifier11
   extends AbstractVerifier
{

   private static final String msgBundle = "EJB11Messages.properties";

   /**
    * Constructs the verifier object.
    *
    * @param   context     context for application information
    */
   public EJBVerifier11(VerificationContext context)
   {
      super( context, new DefaultEventFactory(msgBundle) );
   }

/*
 ***********************************************************************
 *
 *    IMPLEMENTS VERIFICATION STRATEGY INTERFACE
 *
 ***********************************************************************
 */

   /**
    * Verifies the session bean class, home interface and remote interface
    * against the EJB 1.1 specification.
    *
    * @param   session     XML metadata of the session bean
    */
   public void checkSession(SessionMetaData session)
   {
      boolean beanVerified   = false;
      boolean homeVerified   = false;
      boolean remoteVerified = false;

      beanVerified   = verifySessionBean(session);
      homeVerified   = verifySessionHome(session);
      remoteVerified = verifySessionRemote(session);

      if (beanVerified && homeVerified && remoteVerified)
      {
         /*
          * Verification for this session bean done. Fire the event
          * to tell listeneres everything is ok.
          */
         fireBeanVerifiedEvent(session);
      }
   }

   /**
    * Verifies the entity bean class, home interface, remote interface and
    * primary key class against the EJB 1.1 specification.
    *
    * @param   entity      XML metadata of the session bean
    */
   public void checkEntity(EntityMetaData entity)
   {
      boolean pkVerified     = false;
      boolean beanVerified   = false;
      boolean homeVerified   = false;
      boolean remoteVerified = false;

      beanVerified   = verifyEntityBean(entity);
      homeVerified   = verifyEntityHome(entity);
      remoteVerified = verifyEntityRemote(entity);
      pkVerified     = verifyPrimaryKey(entity);

      if ( beanVerified && homeVerified && remoteVerified && pkVerified)
      {
         /*
          * Verification for this entity bean done. Fire the event
          * to tell listeneres everything is ok.
          */
         fireBeanVerifiedEvent(entity);
      }
   }

   public boolean isCreateMethod(Method m) 
   {
      return m.getName().equals(CREATE_METHOD);
   }

   public boolean isEjbCreateMethod(Method m) 
   {
      return m.getName().equals(EJB_CREATE_METHOD);
   }


/*
 *****************************************************************************
 *
 *      VERIFY SESSION BEAN HOME INTERFACE
 *
 *****************************************************************************
 */

   /**
    * Verifies the session bean home interface against the EJB 1.1
    * specification.
    *
    * @param   session     XML metadata of the session bean
    */
   private boolean verifySessionHome(SessionMetaData session)
   {
      /*
       * Indicates whether we issued warnings or not during verification.
       * This boolean is returned to the caller.
       */
      boolean status = true;

      String name = session.getHome();
      if (name == null)
         return false;

      try
      {
         Class home = classloader.loadClass(name);

         /*
          * The home interface of a stateless session bean MUST have one
          * create() method that takes no arguments.
          *
          * The create() method MUST return the session bean's remote
          * interface.
          *
          * There CAN NOT be other create() methods in the home interface.
          *
          * Spec 6.8
          */
         if (session.isStateless())
         {
            if (!hasDefaultCreateMethod(home))
            {
               fireSpecViolationEvent(session, new Section("6.8.a"));
               status = false;
            }
            else
            {
               Method create = getDefaultCreateMethod(home);

               if (!hasRemoteReturnType(session, create))
               {
                  fireSpecViolationEvent(session, create, new Section("6.8.b"));;
                  status = false;
               }

               if (hasMoreThanOneCreateMethods(home))
               {
                  fireSpecViolationEvent(session, new Section("6.8.c"));
                  status = false;
               }
            }
         }

         /*
          * The session bean's home interface MUST extend the
          * javax.ejb.EJBHome interface.
          *
          * Spec 6.10.6
          */
         if (!hasEJBHomeInterface(home))
         {
            fireSpecViolationEvent(session, new Section("6.10.6.a"));
            status = false;
         }

         /*
          * Method arguments defined in the home interface MUST be
          * of valid types for RMI/IIOP.
          *
          * Method return values defined in the home interface MUST
          * be of valid types for RMI/IIOP.
          *
          * Methods defined in the home interface MUST include
          * java.rmi.RemoteException in their throws clause.
          *
          * Spec 6.10.6
          */
         Iterator it = Arrays.asList(home.getMethods()).iterator();
         while (it.hasNext())
         {
            Method method = (Method)it.next();

            if (!hasLegalRMIIIOPArguments(method))
            {
               fireSpecViolationEvent(session, method, new Section("6.10.6.b"));
               status = false;
            }

            if (!hasLegalRMIIIOPReturnType(method))
            {
               fireSpecViolationEvent(session, method, new Section("6.10.6.c"));
               status = false;
            }

            if (!throwsRemoteException(method))
            {
               fireSpecViolationEvent(session, method, new Section("6.10.6.d"));
               status = false;
            }
         }

         /*
          * A session bean's home interface MUST define one or more
          * create(...) methods.
          *
          * Spec 6.10.6
          */
         if (!hasCreateMethod(home))
         {
            fireSpecViolationEvent(session, new Section("6.10.6.e"));
            status = false;
         }

         /*
          * Each create(...) method in the session bean's home interface MUST
          * have a matching ejbCreate(...) method in the session bean's class.
          *
          * Each create(...) method in the session bean's home interface MUST
          * have the same number and types of arguments to its matching
          * ejbCreate(...) method.
          *
          * The return type for a create(...) method MUST be the session
          * bean's remote interface type.
          *
          * All the exceptions defined in the throws clause of the matching
          * ejbCreate(...) method of the enterprise bean class MUST be
          * included in the throws clause of a matching create(...) method.
          *
          * The throws clause of a create(...) method MUST include the
          * javax.ejb.CreateException.
          *
          * Spec 6.10.6
          */
         Iterator createMethods = getCreateMethods(home);
         try
         {
            String beanClass   = session.getEjbClass();
            Class  bean        = classloader.loadClass(beanClass);

            while (createMethods.hasNext())
            {
               Method create = (Method)createMethods.next();

               if (!hasMatchingEJBCreate(bean, create))
               {
                  fireSpecViolationEvent(session, create, new Section("6.10.6.f"));
                  status = false;
               }

               if (!hasRemoteReturnType(session, create))
               {
                  fireSpecViolationEvent(session, create, new Section("6.10.6.g"));
                  status = false;
               }

               if (hasMatchingEJBCreate(bean, create))
               {
                  Method ejbCreate     = getMatchingEJBCreate(bean, create);

                  if (!hasMatchingExceptions(ejbCreate, create))
                  {
                     fireSpecViolationEvent(session, create, new Section("6.10.6.h"));
                     status = false;
                  }
               }

               if (!throwsCreateException(create))
               {
                  fireSpecViolationEvent(session, create, new Section("6.10.6.i"));
                  status = false;
               }
            }
         }
         catch (ClassNotFoundException ignored)
         {}
      }
      catch (ClassNotFoundException e)
      {
         /*
          * The bean provider MUST specify the fully-qualified name of the
          * enterprise bean's  home interface in the <home> element.
          *
          * Spec 16.2
          */
         fireSpecViolationEvent(session, new Section("16.2.c"));
         status = false;
      }

      return status;
   }

/*
 *************************************************************************
 *
 *      VERIFY SESSION BEAN REMOTE INTERFACE
 *
 *************************************************************************
 */
   private boolean verifySessionRemote(SessionMetaData session)
   {
      /*
       * Indicates whether we issued warnings or not during verification.
       * This boolean is returned to the caller.
       */
      boolean status = true;

      String  name   = session.getRemote();
      if (name == null)
         return false;

      try
      {
         Class remote = classloader.loadClass(name);

         /*
          * The remote interface MUST extend the javax.ejb.EJBObject
          * interface.
          *
          * Spec 6.10.5
          */
         if (!hasEJBObjectInterface(remote))
         {
            fireSpecViolationEvent(session, new Section("6.10.5.a"));
            status = false;
         }

         /*
          * Method arguments defined in the remote interface MUST be
          * of valid types for RMI/IIOP.
          *
          * Method return values defined in the remote interface MUST
          * be of valid types for RMI/IIOP.
          *
          * Methods defined in the remote interface MUST include
          * java.rmi.RemoteException in their throws clause.
          *
          * Spec 6.10.5
          */
         Iterator it = Arrays.asList(remote.getMethods()).iterator();
         while (it.hasNext())
         {
            Method method = (Method)it.next();

            if (!hasLegalRMIIIOPArguments(method))
            {
               fireSpecViolationEvent(session, method, new Section("6.10.5.b"));
               status = false;
            }

            if (!hasLegalRMIIIOPReturnType(method))
            {
               fireSpecViolationEvent(session, method, new Section("6.10.5.c"));
               status = false;
            }

            if (!throwsRemoteException(method))
            {
               fireSpecViolationEvent(session, method, new Section("6.10.5.d"));
               status = false;
            }
         }

         /*
          * For each method defined in the remote interface, there MUST be
          * a matching method in the session bean's class. The matching
          * method MUST have:
          *
          *  - the same name
          *  - the same number and types of arguments, and the same
          *    return type
          *  - All the exceptions defined in the throws clause of the
          *    matching method of the session bean class must be defined
          *    in the throws clause of the method of the remote interface
          *
          * Spec 6.10.5
          */
         String beanName   = session.getEjbClass();
         Class  bean       = classloader.loadClass(beanName);

         Iterator iterator = Arrays.asList(remote.getDeclaredMethods()).iterator();
         while (iterator.hasNext())
         {
            Method remoteMethod  = (Method)iterator.next();

            if (!hasMatchingMethod(bean, remoteMethod))
            {
               fireSpecViolationEvent(session, remoteMethod, new Section("6.10.5.e"));
               status = false;
            }

            if (hasMatchingMethod(bean, remoteMethod))
            {
               try
               {
                  Method beanMethod = bean.getMethod(
                     remoteMethod.getName(), remoteMethod.getParameterTypes());

                  if (!hasMatchingReturnType(remoteMethod, beanMethod))
                  {
                     fireSpecViolationEvent(session, remoteMethod, new Section("6.10.5.f"));
                     status = false;
                  }

                  if (!hasMatchingExceptions(beanMethod, remoteMethod))
                  {
                     fireSpecViolationEvent(session, remoteMethod, new Section("6.10.5.g"));
                     status = false;
                  }
               }
               catch (NoSuchMethodException ignored)
               {}
            }
         } // while
      }
      catch (ClassNotFoundException e)
      {
         /*
          * The Bean Provider MUST specify the fully-qualified name of the
          * enterprise bean's remote interface in the <remote> element.
          *
          * Spec 16.2
          */
         fireSpecViolationEvent(session, new Section("16.2.d"));
         status = false;
      }

      return status;
   }

/*
 *************************************************************************
 *
 *      VERIFY SESSION BEAN CLASS
 *
 *************************************************************************
 */
   private boolean verifySessionBean(SessionMetaData session)
   {
      boolean status = true;

      String  name   = session.getEjbClass();

      try
      {
         Class bean = classloader.loadClass(name);

         /*
          * A session bean MUST implement, directly or indirectly,
          * javax.ejb.SessionBean interface.
          *
          * Spec 6.5.1
          * Spec 6.10.2
          */
         if (!hasSessionBeanInterface(bean))
         {
            fireSpecViolationEvent(session, new Section("6.5.1"));
            status = false;
         }

         /*
          * Only a stateful container-managed transaction demarcation
          * session bean MAY implement the SessionSynchronization interface.
          *
          * A stateless Session bean MUST NOT implement the
          * SessionSynchronization interface.
          *
          * Spec 6.5.3
          */
         if (hasSessionSynchronizationInterface(bean))
         {
            if (session.isStateless())
            {
               fireSpecViolationEvent(session, new Section("6.5.3.a"));
               status = false;
            }

            if (session.isBeanManagedTx())
            {
               fireSpecViolationEvent(session, new Section("6.5.3.b"));
               status = false;
            }
         }

         /*
          * A session bean MUST implement AT LEAST one ejbCreate method.
          *
          * Spec 6.5.5
          */
         if (!hasEJBCreateMethod(bean, true))
         {
            fireSpecViolationEvent(session, new Section("6.5.5"));
            status = false;
         }

         /*
          * A session with bean-managed transaction demarcation CANNOT
          * implement the SessionSynchronization interface.
          *
          * Spec 6.6.1 (table 2)
          */
         if(hasSessionSynchronizationInterface(bean)
            && session.isBeanManagedTx())
         {
            fireSpecViolationEvent(session, new Section("6.6.1"));
            status = false;
         }

         /*
          * The session bean class MUST be defined as public.
          *
          * Spec 6.10.2
          */
         if (!isPublic(bean))
         {
            fireSpecViolationEvent(session, new Section("6.10.2.a"));
            status = false;
         }

         /*
          * The session bean class MUST NOT be final.
          *
          * Spec 6.10.2
          */
         if (isFinal(bean))
         {
            fireSpecViolationEvent(session, new Section("6.10.2.b"));
            status = false;
         }

         /*
          * The session bean class MUST NOT be abstract.
          *
          * Spec 6.10.2
          */
         if (isAbstract(bean))
         {
            fireSpecViolationEvent(session, new Section("6.10.2.c"));
            status = false;
         }

         /*
          * The session bean class MUST have a public constructor that
          * takes no arguments.
          *
          * Spec 6.10.2
          */
         if (!hasDefaultConstructor(bean))
         {
            fireSpecViolationEvent(session, new Section("6.10.2.d"));
            status = false;
         }

         /*
          * The session bean class MUST NOT define the finalize() method.
          *
          * Spec 6.10.2
          */
         if (hasFinalizer(bean))
         {
            fireSpecViolationEvent(session, new Section("6.10.2.e"));
            status = false;
         }

         /*
          * The ejbCreate(...) method signatures MUST follow these rules:
          *
          *      - The method MUST be declared as public
          *      - The method MUST NOT be declared as final or static
          *      - The return type MUST be void
          *      - The method arguments MUST be legal types for RMI/IIOP
          *
          * Spec 6.10.3
          */
         if (hasEJBCreateMethod(bean, true))
         {
            Iterator it = getEJBCreateMethods(bean);
            while (it.hasNext())
            {
               Method ejbCreate = (Method)it.next();

               if (!isPublic(ejbCreate))
               {
                  fireSpecViolationEvent(session, ejbCreate, new Section("6.10.3.a"));
                  status = false;
               }

               if ( (isFinal(ejbCreate)) || (isStatic(ejbCreate)) )
               {
                  fireSpecViolationEvent(session, ejbCreate, new Section("6.10.3.b"));
                  status = false;
               }

               if (!hasVoidReturnType(ejbCreate))
               {
                  fireSpecViolationEvent(session, ejbCreate, new Section("6.10.3.c"));
                  status = false;
               }

               if (!hasLegalRMIIIOPArguments(ejbCreate))
               {
                  fireSpecViolationEvent(session, ejbCreate, new Section("6.10.3.d"));
                  status = false;
               }
            } // while
         }
      }
      catch (ClassNotFoundException e)
      {
         /*
          * The Bean Provider MUST specify the fully-qualified name of the
          * Java class that implements the enterprise bean's business
          * methods.
          *
          * Spec 16.2
          */
         fireSpecViolationEvent(session, new Section("16.2.b"));
         status = false;
      }

      return status;
   }


/*
 *************************************************************************
 *
 *      VERIFY ENTITY BEAN HOME INTERFACE
 *
 *************************************************************************
 */

   private boolean verifyEntityHome(EntityMetaData entity)
   {
      boolean status = true;

      String name = entity.getHome();
      if (name == null)
         return false;

      try
      {
         Class home = classloader.loadClass(name);

         /*
          * Entity bean's home interface MUST extend the javax.ejb.EJBHome
          * interface.
          *
          * Spec 9.2.8
          */
         if (!hasEJBHomeInterface(home))
         {
            fireSpecViolationEvent(entity, new Section("9.2.8.a"));
            status = false;
         }

         /*
          * The methods defined in the entity bean's home interface MUST
          * have valid RMI-IIOP argument types.
          *
          * The methods defined in the entity bean's home interface MUST
          * have valid RMI-IIOP return types.
          *
          * The methods defined in the entity bean's home interface MUST
          * have java.rmi.RemoteException in their throws clause.
          *
          * Spec 9.2.8
          */
         Iterator homeMethods = Arrays.asList(home.getMethods()).iterator();
         while (homeMethods.hasNext())
         {
            Method method = (Method)homeMethods.next();

            if (!hasLegalRMIIIOPArguments(method))
            {
               fireSpecViolationEvent(entity, method, new Section("9.2.8.b"));

               status = false;
            }

            if (!hasLegalRMIIIOPReturnType(method))
            {
               fireSpecViolationEvent(entity, method, new Section("9.2.8.c"));
               status = false;
            }

            if (!throwsRemoteException(method))
            {
               fireSpecViolationEvent(entity, method, new Section("9.2.8.d"));
               status = false;
            }
         } // while

         /*
          * Each method defined in the entity bean's home interface must be
          * one of the following:
          *
          *    - a create method
          *    - a finder method
          *
          * Spec 9.2.8
          */
         homeMethods = Arrays.asList(home.getMethods()).iterator();
         while (homeMethods.hasNext())
         {
            Method method = (Method)homeMethods.next();

            // Do not check the methods of the javax.ejb.EJBHome interface
            if (method.getDeclaringClass().getName().equals(EJB_HOME_INTERFACE))
               continue;

            if (! (isCreateMethod(method) || isFinderMethod(method)) )
            {
               fireSpecViolationEvent(entity, method, new Section("9.2.8.e"));
               status = false;
            }
         }

         /*
          * Each create(...) method in the entity bean's home interface MUST
          * have a matching ejbCreate(...) method in the entity bean's class.
          *
          * Each create(...) method in the entity bean's home interface MUST
          * have the same number and types of arguments to its matching
          * ejbCreate(...) method.
          *
          * The return type for a create(...) method MUST be the entity
          * bean's remote interface type.
          *
          * All the exceptions defined in the throws clause of the matching
          * ejbCreate(...) and ejbPostCreate(...) methods of the enterprise
          * bean class MUST be included in the throws clause of a matching
          * create(...) method.
          *
          * The throws clause of a create(...) method MUST include the
          * javax.ejb.CreateException.
          *
          * Spec 9.2.8
          */

         try
         {
            String beanClass   = entity.getEjbClass();
            Class  bean        = classloader.loadClass(beanClass);

            Iterator createMethods = getCreateMethods(home);
            while (createMethods.hasNext())
            {
               Method create = (Method)createMethods.next();

               if (!hasMatchingEJBCreate(bean, create))
               {
                  fireSpecViolationEvent(entity, create, new Section("9.2.8.f"));
                  status = false;
               }

               if (!hasRemoteReturnType(entity, create))
               {
                  fireSpecViolationEvent(entity, create, new Section("9.2.8.g"));
                  status = false;
               }

               if (hasMatchingEJBCreate(bean, create)
                  && hasMatchingEJBPostCreate(bean, create))
               {
                  Method ejbCreate     = getMatchingEJBCreate(bean, create);
                  Method ejbPostCreate = getMatchingEJBPostCreate(bean, create);

                  if ( !(hasMatchingExceptions(ejbCreate, create)
                     && hasMatchingExceptions(ejbPostCreate, create)) )
                  {
                     fireSpecViolationEvent(entity, create, new Section("9.2.8.h"));
                     status = false;
                  }
               }

               if (!throwsCreateException(create))
               {
                  fireSpecViolationEvent(entity, create, new Section("9.2.8.i"));
                  status = false;
               }
            }
         }
         catch (ClassNotFoundException ignored)
         {}

         /*
          * Each finder method MUST match one of the ejbFind<METHOD> methods
          * defined in the entity bean class.
          *
          * The matching ejbFind<METHOD> method MUST have the same number and
          * types of arguments.
          *
          * The return type for a find<METHOD> method MUST be the entity
          * bean's remote interface type (single-object finder) or a
          * collection thereof (for a multi-object finder).
          *
          * All the exceptions defined in the throws clause of an ejbFind
          * method of the entity bean class MUST be included in the throws
          * clause of the matching find method of the home interface.
          *
          * The throws clause of a finder method MUST include the
          * javax.ejb.FinderException.
          *
          * Spec 9.2.8
          */
         try
         {
            String beanClass   = entity.getEjbClass();
            Class  bean        = classloader.loadClass(beanClass);

            Iterator finderMethods = getFinderMethods(home);
            while (finderMethods.hasNext())
            {
               Method finder = (Method)finderMethods.next();

               if ((entity.isBMP()) && (!hasMatchingEJBFind(bean, finder)))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.8.j"));
                  status = false;
               }

               if (!(hasRemoteReturnType(entity, finder)
                  || isMultiObjectFinder(finder)))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.8.k"));
                  status = false;
               }

               if ((entity.isBMP()) && (hasMatchingEJBFind(bean, finder)))
               {
                  Method ejbFind  = getMatchingEJBFind(bean, finder);
                  if ( !(hasMatchingExceptions(ejbFind, finder)))
                  {
                     fireSpecViolationEvent(entity, finder, new Section("9.2.8.l"));
                     status = false;
                  }
               }

               if (!throwsFinderException(finder))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.8.m"));
                  status = false;
               }
            }
         }
         catch (ClassNotFoundException ignored)
         {}
      }
      catch (ClassNotFoundException e)
      {
         /*
          * The bean provider MUST specify the fully-qualified name of the
          * enterprise bean's  home interface in the <home> element.
          *
          * Spec 16.2
          */
         fireSpecViolationEvent(entity, new Section("16.2.c"));
         status = false;
      }

      return status;
   }


/*
 *************************************************************************
 *
 *      VERIFY ENTITY BEAN REMOTE INTERFACE
 *
 *************************************************************************
 */

   private boolean verifyEntityRemote(EntityMetaData entity)
   {
      boolean status = true;

      String  name   = entity.getRemote();
      if (name == null)
         return false;

      try
      {
         Class remote = classloader.loadClass(name);

         /*
          * Entity bean's remote interface MUST extend
          * the javax.ejb.EJBObject interface.
          *
          * Spec 9.2.7
          */
         if (!hasEJBObjectInterface(remote))
         {
            fireSpecViolationEvent(entity, new Section("9.2.7.a"));
            status = false;
         }

         /*
          * The methods defined in the entity bean's remote interface MUST
          * have valid RMI-IIOP argument types.
          *
          * The methods defined in the entity bean's home interface MUST
          * have valid RMI-IIOP return types.
          *
          * The methods defined in the entity bean's home interface MUST
          * have java.rmi.RemoteException in their throws clause.
          *
          * Spec 9.2.7
          */
         Iterator remoteMethods = Arrays.asList(remote.getMethods()).iterator();
         while (remoteMethods.hasNext())
         {
            Method method = (Method)remoteMethods.next();

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
         } // while

         /*
          * For each method defined in the remote interface, there MUST be
          * a matching method in the entity bean's class. The matching
          * method MUST have:
          *
          *     - The same name.
          *     - The same number and types of its arguments.
          *     - The same return type.
          *     - All the exceptions defined in the throws clause of the
          *       matching method of the enterprise Bean class must be
          *       defined in the throws clause of the method of the remote
          *       interface.
          *
          * Spec 9.2.7
          */

         try
         {
            String beanClass   = entity.getEjbClass();
            Class  bean        = classloader.loadClass(beanClass);

            remoteMethods = Arrays.asList(remote.getMethods()).iterator();
            while (remoteMethods.hasNext())
            {
               Method method = (Method)remoteMethods.next();

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
                     Method beanMethod = bean.getMethod(method.getName(), method.getParameterTypes());

                     if (!hasMatchingReturnType(beanMethod, method))
                     {
                        fireSpecViolationEvent(entity, method, new Section("9.2.7.f"));
                        status = false;
                     }

                     if (!hasMatchingExceptions(beanMethod, method))
                     {
                        fireSpecViolationEvent(entity, method, new Section("9.2.7.g"));
                        status = false;
                     }
                  }
                  catch (NoSuchMethodException ignored)
                  {}
               }
            }
         }
         catch (ClassNotFoundException ignored)
         {}
      }
      catch (ClassNotFoundException e)
      {
         /*
          * The Bean Provider MUST specify the fully-qualified name of the
          * enterprise bean's remote interface in the <remote> element.
          *
          * Spec 16.2
          */
         fireSpecViolationEvent(entity, new Section("16.2.d"));
         status = false;
      }

      return status;
   }

/*
 *************************************************************************
 *
 *      VERIFY ENTITY BEAN CLASS
 *
 *************************************************************************
 */

   private boolean verifyEntityBean(EntityMetaData entity)
   {
      boolean status = true;

      String  name   = entity.getEjbClass();

      try
      {
         Class bean = classloader.loadClass(name);

         /*
          * The enterprise bean class MUST implement, directly or
          * indirectly, the javax.ejb.EntityBean interface.
          *
          * Spec 9.2.2
          */
         if (!hasEntityBeanInterface(bean))
         {
            fireSpecViolationEvent(entity, new Section("9.2.2.a"));
            status = false;
         }

         /*
          * The entity bean class MUST be defined as public.
          *
          * Spec 9.2.2
          */
         if (!isPublic(bean))
         {
            fireSpecViolationEvent(entity, new Section("9.2.2.b"));
            status = false;
         }

         /*
          * The entity bean class MUST NOT be defined as abstract.
          *
          * Spec 9.2.2
          */
         if (isAbstract(bean))
         {
            fireSpecViolationEvent(entity, new Section("9.2.2.c"));
            status = false;
         }

         /*
          * The entity bean class MUST NOT be defined as final.
          *
          * Spec 9.2.2
          */
         if (isFinal(bean))
         {
            fireSpecViolationEvent(entity, new Section("9.2.2.d"));
            status = false;
         }

         /*
          * The entity bean class MUST define a public constructor that
          * takes no arguments
          *
          * Spec 9.2.2
          */
         if (!hasDefaultConstructor(bean))
         {
            fireSpecViolationEvent(entity, new Section("9.2.2.e"));
            status = false;
         }

         /*
          * The entity bean class MUST NOT define the finalize() method.
          *
          * Spec 9.2.2
          */
         if (hasFinalizer(bean))
         {
            fireSpecViolationEvent(entity, new Section("9.2.2.f"));
            status = false;
         }

         /*
          * The ejbCreate(...) method signatures MUST follow these rules:
          *
          *      - The method MUST be declared as public
          *      - The method MUST NOT be declared as final or static
          *      - The return type MUST be the entity bean's primary key type
          *      - The method arguments MUST be legal types for RMI/IIOP
          *      - The method return value type MUST be legal type for RMI/IIOP
          *
          * Spec 9.2.3
          */
         if (hasEJBCreateMethod(bean, false))
         {
            Iterator it = getEJBCreateMethods(bean);
            while (it.hasNext())
            {
               Method ejbCreate = (Method)it.next();

               if (!isPublic(ejbCreate))
               {
                  fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.a"));
                  status = false;
               }

               if ( (isFinal(ejbCreate)) || (isStatic(ejbCreate)) )
               {
                  fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.b"));
                  status = false;
               }

               if (!hasPrimaryKeyReturnType(entity, ejbCreate))
               {
                  fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.c"));
                  status = false;
               }

               if (!hasLegalRMIIIOPArguments(ejbCreate))
               {
                  fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.d"));
                  status = false;
               }

               if (!hasLegalRMIIIOPReturnType(ejbCreate))
               {
                  fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.3.e"));
                  status = false;
               }
            }
         }

         /*
          * For each ejbCreate(...) method, the entity bean class MUST
          * define a matching ejbPostCreate(...) method.
          *
          * The ejbPostCreate(...) method MUST follow these rules:
          *
          *   - the method MUST be declared as public
          *   - the method MUST NOT be declared as final or static
          *   - the return type MUST be void
          *   - the method arguments MUST be the same as the matching
          *     ejbCreate(...) method
          *
          * Spec 9.2.4
          */
         if (hasEJBCreateMethod(bean, false))
         {
            Iterator it =  getEJBCreateMethods(bean);
            while (it.hasNext())
            {
               Method ejbCreate = (Method)it.next();

               if (!hasMatchingEJBPostCreate(bean, ejbCreate))
               {
                  fireSpecViolationEvent(entity, ejbCreate, new Section("9.2.4.a"));
                  status = false;
               }

               if (hasMatchingEJBPostCreate(bean, ejbCreate))
               {
                  Method ejbPostCreate = getMatchingEJBPostCreate(bean, ejbCreate);

                  if (!isPublic(ejbPostCreate))
                  {
                     fireSpecViolationEvent(entity, ejbPostCreate, new Section("9.2.4.b"));
                     status = false;
                  }

                  if (isStatic(ejbPostCreate))
                  {
                     fireSpecViolationEvent(entity, ejbPostCreate, new Section("9.2.4.c"));
                     status = false;
                  }

                  if (isFinal(ejbPostCreate))
                  {
                     fireSpecViolationEvent(entity, ejbPostCreate, new Section("9.2.4.d"));
                     status = false;
                  }

                  if(!hasVoidReturnType(ejbPostCreate))
                  {
                     fireSpecViolationEvent(entity, ejbPostCreate, new Section("9.2.4.e"));
                     status = false;
                  }
               }
            }
         }

         /*
          * Every entity bean MUST define the ejbFindByPrimaryKey method.
          *
          * The return type for the ejbFindByPrimaryKey method MUST be the
          * primary key type.
          *
          * The ejbFindByPrimaryKey method MUST be a single-object finder.
          *
          * Spec 9.2.5
          */
         if (entity.isBMP() && (!hasEJBFindByPrimaryKey(bean)))
         {
            fireSpecViolationEvent(entity, new Section("9.2.5.a"));
            status = false;
         }

         if (hasEJBFindByPrimaryKey(bean))
         {
            Method ejbFindByPrimaryKey = getEJBFindByPrimaryKey(bean);

            if (!hasPrimaryKeyReturnType(entity, ejbFindByPrimaryKey))
            {
               fireSpecViolationEvent(entity, ejbFindByPrimaryKey, new Section("9.2.5.b"));
               status = false;
            }

            if (!isSingleObjectFinder(entity, ejbFindByPrimaryKey))
            {
               fireSpecViolationEvent(entity, ejbFindByPrimaryKey, new Section("9.2.5.c"));
               status = false;
            }
         }

         /*
          * A finder method MUST be declared as public.
          *
          * A finder method MUST NOT be declared as static.
          *
          * A finder method MUST NOT be declared as final.
          *
          * The finder method argument types MUST be legal types
          * for RMI/IIOP
          *
          * The finder method return type MUST be either the entity bean's
          * primary key type, or java.lang.util.Enumeration interface or
          * java.lang.util.Collection interface.
          *
          * Spec 9.2.5
          */
         if (hasFinderMethod(bean))
         {
            Iterator it = getEJBFindMethods(bean);
            while (it.hasNext())
            {
               Method finder = (Method)it.next();

               if (!isPublic(finder))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.5.d"));
                  status = false;
               }

               if (isFinal(finder))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.5.e"));
                  status = false;
               }

               if (isStatic(finder))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.5.f"));
                  status = false;
               }

               if (!hasLegalRMIIIOPArguments(finder))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.5.g"));
                  status = false;
               }

               if (! (isSingleObjectFinder(entity, finder)
                  || isMultiObjectFinder(finder)))
               {
                  fireSpecViolationEvent(entity, finder, new Section("9.2.5.h"));
                  status = false;
               }
            }
         }
      }
      catch (ClassNotFoundException e)
      {
         /*
          * The Bean Provider MUST specify the fully-qualified name of the
          * Java class that implements the enterprise bean's business
          * methods.
          *
          * Spec 16.2
          */
         fireSpecViolationEvent(entity, new Section("16.2.b"));
         status = false;
      }

      return status;
   }

   private boolean verifyPrimaryKey(EntityMetaData entity)
   {
      boolean status = true;

      if(entity.getPrimaryKeyClass() == null
         || entity.getPrimaryKeyClass().length() == 0)
      {
         fireSpecViolationEvent(entity, new Section("16.5.a"));
         return false; // We can't get any further if there's no PK class specified!
      }

      if(entity.getPrimKeyField() == null
         || entity.getPrimKeyField().length() == 0)
      {
         Class cls = null;

         try
         {
            cls = classloader.loadClass(entity.getPrimaryKeyClass());

            if(entity.isCMP())
            {
               if(!isPublic(cls))
               {
                  fireSpecViolationEvent(entity, new Section("9.4.7.2.a"));
                  status = false;
               }

               if(!isAllFieldsPublic(cls))
               {
                  fireSpecViolationEvent(entity, new Section("9.4.7.2.b"));
                  status = false;
               }

               if(!hasANonStaticField(cls))
               {
                  fireSpecViolationEvent(entity, new Section("9.4.7.2.c"));
                  status = false;
               }
            }

            if(!cls.getName().equals("java.lang.Object"))
            {
               Object one, two;
               try
               {
                  one = cls.newInstance();
                  two = cls.newInstance();
                  try
                  {
                     if(!one.equals(two))
                     {
                        fireSpecViolationEvent(entity, new Section("9.2.9.b"));
                        status = false;
                     }
                  }
                  catch(NullPointerException e)
                  {} // That's OK - the implementor expected the fields to have values
                  try
                  {
                     if(one.hashCode() != two.hashCode())
                     {
                        fireSpecViolationEvent(entity, new Section("9.2.9.c"));
                        status = false;
                     }
                  }
                  catch(NullPointerException e)
                  {} // That's OK - the implementor expected the fields to have values
               }
               catch(IllegalAccessException e)
               {
                  // [FIXME] The two error messages below are incorrect.
                  //         The RMI-IDL language mapping does not require
                  //         the value types to have a no args constructor.
                  //                                                  [JPL]
                  //
                  //fireSpecViolationEvent(entity, new Section("9.2.9.a"));
                  //status = false;
               }
               catch(InstantiationException e)
               {
                  //fireSpecViolationEvent(entity, new Section("9.2.9.a"));
                  //status = false;
               }
            }
         }
         catch(ClassNotFoundException e)
         {
            fireSpecViolationEvent(entity, new Section("16.2.e"));
            status = false;  // Can't do any other checks if the class is null!
         }
      }
      else
      {
         if(entity.isBMP())
         {
            fireSpecViolationEvent(entity, new Section("9.4.7.1.a"));
            status = false;
         }

         try
         {
            Class fieldClass = classloader.loadClass(entity.getEjbClass());
            Field field = null;
            try
            {
               field = fieldClass.getField(entity.getPrimKeyField());
               if(!entity.getPrimaryKeyClass().equals(field.getType().getName()))
               {
                  fireSpecViolationEvent(entity, new Section("9.4.7.1.c"));
                  status = false;
               }

               Iterator it = entity.getCMPFields();
               boolean found = false;
               while(it.hasNext())
               {
                  String fieldName = (String)it.next();
                  if(fieldName.equals(entity.getPrimKeyField()))
                  {
                     found = true;
                     break;
                  }
               }

               if(!found)
               {
                  fireSpecViolationEvent(entity, new Section("9.4.7.1.d"));
                  status = false;
               }
            }
            catch(NoSuchFieldException e)
            {
               fireSpecViolationEvent(entity, new Section("9.4.7.1.b"));
               status = false;
            }
         }
         catch(ClassNotFoundException e)
         {} // reported elsewhere
      }

      return status;
   }

}
/*
vim:ts=3:sw=3:et
*/
