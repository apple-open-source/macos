package org.jboss.test.bench.interfaces;

import java.io.Serializable;
import java.io.IOException;

public class AComplexPK implements Serializable{

    public boolean aBoolean;
    public int anInt;
    public long aLong;
    public double aDouble;
    public String aString;

    public AComplexPK() {};
    

    public AComplexPK(boolean aBoolean, int anInt, long aLong, double aDouble, String aString) {

        this.aBoolean = aBoolean;
        this.anInt = anInt;
        this.aLong = aLong;
        this.aDouble = aDouble;
        this.aString = aString;
    }
	
	public boolean equals(Object other) {
		if (other != null && other instanceof AComplexPK) {
			AComplexPK otherPK = (AComplexPK)other;
			return ((aBoolean == otherPK.aBoolean) &&
				(anInt == otherPK.anInt) &&
				(aLong == otherPK.aLong) &&
				(aDouble == otherPK.aDouble) &&
				(aString == null ? otherPK.aString == null : aString.equals(otherPK.aString)));
		} else return false;
	}
				
	
	public int hashCode() {
		
		// Missing the double but ok for test
		
		return anInt*
				(new Long(aLong)).intValue()*
				(new Double(aDouble)).intValue()*
				aString.hashCode();
	}
} 

