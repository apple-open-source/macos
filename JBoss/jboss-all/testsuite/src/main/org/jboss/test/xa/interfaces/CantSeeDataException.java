package org.jboss.test.xa.interfaces;

public class CantSeeDataException extends Exception {

    public CantSeeDataException() {
        super();
    }
    public CantSeeDataException(String message) {
        super(message);
    }
}
