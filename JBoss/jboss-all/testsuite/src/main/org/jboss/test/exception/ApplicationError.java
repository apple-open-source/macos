package org.jboss.test.exception;

public class ApplicationError extends Error {
   public ApplicationError(String message) {
      super(message);
   }
}

