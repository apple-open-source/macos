package org.jboss.test.dbtest.interfaces;

import java.io.Serializable;
import java.io.IOException;

public class MyObject implements Serializable {

    public String aString;
	

    public MyObject() { 
		aString = "dummy";
	}
    
} 
