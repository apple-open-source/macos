
package org.jboss.test.jmx.xmbean;

import java.util.ArrayList;
import java.util.Collection;

/**
 * for some reason this doesn't work:
 *            descriptor="name=\"testdescriptor\" value=\"testvalue\""
 *
 * @jmx.mbean description="sample for jboss xmbean.dtd"
 *            persistPolicy="Never"
 *            persistPeriod="10"
 *            persistLocation="pl1"
 *            persistName="JBossXMLExample1"
 *            currencyTimeLimit="10"
 *            state-action-on-update="restart"
 *
 * @jmx.managed-attribute
 *    name="ArtificialAttribute"
 *    description="artificial attribute not impemeneted in class"
 *    type="java.lang.String"
 *    currencyTimeLimit="-1"
 *
 * @jmx.notification description="first notification"
 *                   name="javax.management.SomeEvent"
 *                   notificationType="xd.example.first,xd.example.second"
 *                   persistPolicy="Never"
 *                   persistPeriod="20"
 *                   persistLocation="pl2"
 *                   persistName="JBossXMLExample2"
 *                   currencyTimeLimit="20"
 *
 * @jboss.xmbean
 **/
public class User {

   private long id          = System.currentTimeMillis();
   private int number;
   private String name      = "";
   private String address   = "";
   private String password  = null;
   //private String[] numbers = new String[3];
   private Collection numbers = new ArrayList();

   /**
    * Creates a new <code>User</code> instance using constructor with one argument.
    *
    * @param id a <code>long</code> value
    * @jmx.managed-constructor
    */
   public User(long id)
   {
      this.id = id;
   }

   /**
    * Creates a new <code>User</code> using constructor with no argument
    * @jmx.managed-constructor
    *
    */
   public User()
   {
   }


   /**
    * Describe <code>getID</code> method here.
    * read-only attribute
    * @return a <code>long</code> value
    * @jmx.managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    *            access="read-only"
    */
   public long getID() {
        return id;
    }



    /**
     * Describe <code>setID</code> method here.
     * application method, not exposed to management
     *
     * @param id a <code>long</code> value
     */
   public void setID(long id) {
        this.id = id;
    }




   /**
    * mbean get-set pair for field number
    * Get the value of number
    * @return value of number
    *
    * @jmx.managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    *            value="5"
    */
   public int getNumber()
   {
      return number;
   }


   /**
    * Set the value of number
    * @param number  Value to assign to number
    *
    * @jmx.managed-attribute
    */
   public void setNumber(int number)
   {
      this.number = number;
   }



   /**
    * Describe <code>getName</code> method here.
    * read-write attribute
    * @return a <code>String</code> value
    * @jmx.managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    *            value="test name"
    */
   public String getName() {
        return name;
    }
   /**
    * Describe <code>setName</code> method here.
    *
    * @param name a <code>String</code> value
    * @jmx.managed-attribute
    */
   public void setName(String name) {
      this.name = name;
   }



   /**
    * Describe <code>getAddress</code> method here.
    * read-write attribute
    * @return a <code>String</code> value
    * @jmx.managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    *            value="somewhere"
    */
   public String getAddress() {
        return address;
    }

   /**
    * Describe <code>setAddress</code> method here.
    *
    * @param address a <code>String</code> value
    * @jmx.managed-attribute
    */
   public void setAddress(String address) {
        this.address = address;
    }



   /**
    * Describe <code>getPhoneNumbers</code> method here.
    * read-write attribute
    * @return a <code>String[]</code> value
    * @jmx.managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    */
   public Collection getPhoneNumbers() {
      return numbers;
   }
   /**
    * Describe <code>setPhoneNumbers</code> method here.
    *
    * @param numbers a <code>String[]</code> value
    * @jmx.managed-attribute
    */
   public void setPhoneNumbers(Collection numbers) {
      this.numbers.clear();
      this.numbers.addAll(numbers);
   }



    /**
     * Describe <code>setPassword</code> method here.
     * write only attribute
     * @param passwd a <code>String</code> value
    * @jmx.managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
     */
   public void setPassword(String passwd) {
        this.password = passwd;
    }


    // management operations

    /**
     * Describe <code>printInfo</code> method here.
     * prints info
     * @return a <code>String</code> value
    * @jmx.managed-operation
     */
   public String printInfo() {
        return
          "User: " + getName() +"\n"+
          "Address: " + getAddress() +"\n"+
           "Phone numbers: " + numbers;
    }

   /**
    * Describe <code>addPhoneNumber</code> method here.
    *
    * @param number a <code>String</code> value, the  phone number to add
    * @jmx.managed-operation
    * @jmx.managed-parameter name="number" type="java.lang.String"
    */
   public void addPhoneNumber(String number) {
      numbers.add(number);
   }

   /**
    * Describe <code>removePhoneNumber</code> method here.
    *
    * @param index an <code>int</code> value, the index of phone number to remove
    * @jmx.managed-operation
    * @jmx.managed-parameter name="number" type="java.lang.String"
    */
   public void removePhoneNumber(String number) {
      numbers.remove(number);
   }
}


