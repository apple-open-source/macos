
package test.implementation.modelmbean.support;
/**
 * @jmx:mbean description="sample for jboss xmbean.dtd"
 *            persistPolicy="Never"
 *            persistPeriod="10"
 *            persistLocation="pl1"
 *            persistName="JBossXMLExample1"
 *            currencyTimeLimit="10"
 *            descriptor="name=\"testdescriptor\" value=\"testvalue\""
 *            state-action-on-update="RESTART"
 *            
 *
 * @jmx:notification description="first notification"
 *                   name="javax.management.SomeEvent"
 *                   notificationType="xd.example.first,xd.example.second"
 *                   persistPolicy="Never"
 *                   persistPeriod="20"
 *                   persistLocation="pl2"
 *                   persistName="JBossXMLExample2"
 *                   currencyTimeLimit="20"
 *   
 **/                
public class User {

    private long id          = System.currentTimeMillis();
    private String name      = "";
    private String address   = "";
    private String password  = null;
    private String[] numbers = new String[3];
    
   /**
    * Creates a new <code>User</code> instance using constructor with one argument.
    *
    * @param id a <code>long</code> value
    * @jmx:managed-constructor
    */
   public User(long id)
   {
      this.id = id;
   }

   /**
    * Creates a new <code>User</code> using constructor with no argument
    * @jmx:managed-constructor
    *
    */
   public User()
   {
   }


   /**
    * Describe <code>getID</code> method here.
    * read-only attribute
    * @return a <code>long</code> value
    * @jmx:managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
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
    * Describe <code>getName</code> method here.
    * read-write attribute
    * @return a <code>String</code> value
    * @jmx:managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    */
   public String getName() {
        return name;
    }
   /**
    * Describe <code>setName</code> method here.
    *
    * @param name a <code>String</code> value
    * @jmx:managed-attribute
    */
   public void setName(String name) {
        
      //System.out.println("SetNAME");

      this.name = name;
   }

    

   /**
    * Describe <code>getAddress</code> method here.
    * read-write attribute
    * @return a <code>String</code> value
    * @jmx:managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    */
   public String getAddress() {
        return address;
    }

   /**
    * Describe <code>setAddress</code> method here.
    *
    * @param address a <code>String</code> value
    * @jmx:managed-attribute
    */
   public void setAddress(String address) {
        this.address = address;
    }

    

   /**
    * Describe <code>getPhoneNumbers</code> method here.
    * read-write attribute
    * @return a <code>String[]</code> value
    * @jmx:managed-attribute persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    */
   public String[] getPhoneNumbers() {
        return numbers;
    }
   /**
    * Describe <code>setPhoneNumbers</code> method here.
    *
    * @param numbers a <code>String[]</code> value
    * @jmx:managed-attribute
    */
   public void setPhoneNumbers(String[] numbers) {
        this.numbers = numbers;
    }

    

    /**
     * Describe <code>setPassword</code> method here.
     * write only attribute
     * @param passwd a <code>String</code> value
    * @jmx:managed-attribute persistPolicy="Never"
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
    * @jmx:managed-operation
     */
   public String printInfo() {
        return 
          "User: " + getName() +"\n"+
          "Address: " + getAddress() +"\n"+
          "Phone #: " + getPhoneNumbers()[0] +"\n"+
          "Phone #: " + getPhoneNumbers()[1] +"\n"+
          "Phone #: " + getPhoneNumbers()[2] +"\n";
    }
    
   /**
    * Describe <code>addPhoneNumber</code> method here.
    *
    * @param number a <code>String</code> value, the  phone number to add
    * @jmx:managed-operation
    */
   public void addPhoneNumber(String number) {
        for (int i = 0; i < numbers.length; ++i)
            if (numbers[i] == null) {
                numbers[i] = number;
                break;
            }
    }

   /**
    * Describe <code>removePhoneNumber</code> method here.
    *
    * @param index an <code>int</code> value, the index of phone number to remove
    * @jmx:managed-operation
    */
   public void removePhoneNumber(int index) {
        if (index < 0 || index >= numbers.length)
            return;
            
        numbers[index] = null;
    }
}


