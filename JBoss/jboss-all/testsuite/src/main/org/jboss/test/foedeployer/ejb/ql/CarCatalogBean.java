/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.foedeployer.ejb.ql;


import java.sql.Date;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.Iterator;
import java.util.ArrayList;

import java.rmi.RemoteException;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.EJBException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import javax.rmi.PortableRemoteObject;

import org.apache.log4j.Category;

/**
 * Car catalog session bean.
 *
 * @ejb.bean
 *    type="Stateless"
 *    name="CarCatalog"
 *    jndi-name="CarCatalogEJB.CarCatalogHome"
 *    generate="true"
 *
 * @ejb.ejb-ref
 *    ejb-name="Car"
 *    view-type="local"
 *
 * @ejb.transaction type="Required"
 */
public class CarCatalogBean
   implements SessionBean
{
   // Attributes --------------------------------------------------
   static Category log = Category.getInstance( CarCatalogBean.class );
   static String CAR_NAME = "java:comp/env/ejb/Car";
   private CarLocalHome carHome;

   // Business methods ---------------------------------------------
   /**
    * Creates a car
    *
    * @ejb.interface-method
    */
   public void createCar( String number, String color, int year )
   {
      try
      {
         carHome.create( number, color, year );
      }
      catch( CreateException ce )
      {
         log.debug( "Exception in create(): ", ce );
         throw new EJBException( ce );
      }
   }

   /**
    * Removes a car if exists
    *
    * @ejb.interface-method
    */
   public void removeCarIfExists( String number )
   {
      try
      {
         CarLocal car = carHome.findByPrimaryKey( number );
         car.remove();
      }
      catch( Exception e )
      {
         log.debug( "Exception while removing car with number "
            + number + ": ", e );
      }
   }

   /**
    * Finds all car numbers
    *
    * @ejb.interface-method
    */
   public Collection getAllCarNumbers()
   {
      Collection allNumbers = new ArrayList();
      try
      {
         Collection allCars = carHome.findAll();
         for( Iterator iter = allCars.iterator(); iter.hasNext(); )
         {
            CarLocal car = (CarLocal) iter.next();
            allNumbers.add( car.getNumber() );
         }
      }
      catch( Exception e )
      {
         log.debug( "Exception in getAllCarNumbers(): ", e );
         throw new EJBException( e );
      }
      return allNumbers;
   }

   /**
    * Finds car numbers by color
    *
    * @ejb.interface-method
    */
   public Collection getCarsWithColor( String color )
   {
      Collection result = new ArrayList();
      try
      {
         Collection cars = carHome.findByColor( color );
         for( Iterator iter = cars.iterator(); iter.hasNext(); )
         {
            CarLocal car = (CarLocal) iter.next();
            result.add( car.getNumber() );
         }
      }
      catch( Exception e )
      {
         log.debug( "Exception in getCarsWithColor(): ", e );
         throw new EJBException( e );
      }
      return result;
   }

   /**
    * Finds car made after year
    *
    * @ejb.interface-method
    */
   public Collection getCarsAfterYear( int year )
   {
      Collection result = new ArrayList();
      try
      {
         Collection cars = carHome.findAfterYear( year );
         for( Iterator iter = cars.iterator(); iter.hasNext(); )
         {
            CarLocal car = (CarLocal) iter.next();
            result.add( car.getNumber() );
         }
      }
      catch( Exception e )
      {
         log.debug( "Exception in getCarsAfterYear(): ", e );
         throw new EJBException( e );
      }
      return result;
   }


   // SessionBean implementation -------------------------------------
 
   public void setSessionContext(SessionContext c)
   {
      try
      {
         Context ic = new InitialContext();       
         carHome = (CarLocalHome)ic.lookup(CAR_NAME);
      }
      catch(NamingException ne)
      {
         throw new EJBException(ne);
      }
   }

   /**
    * create method
    *
    * @ejb:create-method
    */ 
   public void ejbCreate() { }
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbRemove() { }
}
