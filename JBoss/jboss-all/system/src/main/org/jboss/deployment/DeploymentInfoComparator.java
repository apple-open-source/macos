package org.jboss.deployment;

import java.util.Comparator;

/**
 * This comparator takes a delegate comparator that can compare URLs, and 
 * applies that to DeploymentInfo objects
 */
public class DeploymentInfoComparator implements Comparator {
    
    /** the delegate URL comparator */
    private Comparator urlComparator;
    
    public DeploymentInfoComparator(Comparator comp) {
        this.urlComparator = comp;
    }
    
    public int compare(Object o1, Object o2) {
        return urlComparator.compare(((DeploymentInfo)o1).url, ((DeploymentInfo)o2).url);
    }
}
