package org.jboss.test.testbean2.interfaces;

import java.io.Serializable;
import java.io.IOException;

public class MyObject implements Serializable {

    public String aString;
	

    public MyObject() { 
		aString = "dummy";
	}
    
} 
