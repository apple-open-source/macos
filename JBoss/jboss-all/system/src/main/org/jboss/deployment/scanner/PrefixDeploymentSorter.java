package org.jboss.deployment.scanner;

import java.util.Comparator;
import java.net.URL;
import org.jboss.deployment.DeploymentSorter;

/**
 * <p>This class is a comparator to sort deployment URLs based on the existence
 * of a numeric prefix.  The name portion of the URL is evaluated for any 
 * leading digits.  If they exist, then they will define a numerical ordering
 * for this comparator.  If there is no leading digits, then they will
 * compare as less than any name with leading digits.  In the case of a 
 * tie, the DeploymentSorter is consulted (@see org.jboss.deployment.DeploymentSorter).
 * 
 * <p>Ex.these names are in ascending order:
 * test.sar, crap.ear, 001test.jar, 5test.rar, 5foo.jar, 120bar.jar
 */
public class PrefixDeploymentSorter implements Comparator {
    
    /** This is used to break ties */
    private DeploymentSorter sorter = new DeploymentSorter();
    
    /** 
     * As described in @see java.util.Comparator.  This implements the
     * comparison technique described above.
     */
    public int compare(Object o1, Object o2) {
        int comp = getPrefixValue((URL)o1) - getPrefixValue((URL)o2);
        
        return comp == 0 ? sorter.compare(o1, o2) : comp;
    }
    
    /**
     * This extracts the prefix value from the name of a URL.  If no prefix
     * value exists, this returns -1
     */
    private int getPrefixValue(URL url) {
        String path = url.getPath();
        int nameEnd = path.length() - 1;
        if (nameEnd <= 0) {
            return 0;
        }
        
        // ignore a trailing '/'
        if (path.charAt(nameEnd) == '/') {
            nameEnd--;
        }
        
        // find the previous URL separator: '/'
        int nameStart = path.lastIndexOf('/', nameEnd) + 1;
        
        // calculate where the digit-prefix ends
        int prefixEnd = nameStart;
        while (prefixEnd <= nameEnd && Character.isDigit(path.charAt(prefixEnd))) {
            prefixEnd++;
        }
        
        // If zero length prefix, return -1
        if (prefixEnd == nameStart) {
            return -1;
        }
        
        // strip leading zeroes
        while (nameStart < prefixEnd && path.charAt(nameStart) == '0') {
            nameStart++;
        }
        
        return (nameStart == prefixEnd) ? 0 : Integer.parseInt(path.substring(nameStart, prefixEnd));
    }
}
