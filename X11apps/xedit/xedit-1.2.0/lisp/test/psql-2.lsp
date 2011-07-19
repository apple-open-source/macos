;; Postgresql C library interface, example program 2, using the xedit
;; lisp interface

;;  Test of the asynchronous notification interface
;;
;; Start this program, then from psql in another window do
;;   NOTIFY TBL2;
;;
;; Or, if you want to get fancy, try this:
;; Populate a database with the following:
;;
;;   CREATE TABLE TBL1 (i int4);
;;
;;   CREATE TABLE TBL2 (i int4);
;;
;;   CREATE RULE r1 AS ON INSERT TO TBL1 DO
;;     (INSERT INTO TBL2 values (new.i); NOTIFY TBL2);
;;
;; and do
;;
;;   INSERT INTO TBL1 values (10);
(require "psql")

(defun exit-nicely (conn)
    (pq-finish conn)
    (quit 1)
)

;; begin, by setting the parameters for a backend connection if the
;; parameters are null, then the system will try to use reasonable
;; defaults by looking up environment variables or, failing that,
;; using hardwired constants
(setq pghost nil)		; host name of the backend server
(setq pgport nil)		; port of the backend server
(setq pgoptions nil)		; special options to start up the backend server
(setq pgtty nil)		; debugging tty for the backend server
(setq pgdbname "test")		; change this to the name of your test database
				;; XXX Note: getenv not yet implemented in the
				 ; lisp interpreter

;; make a connection to the database
(setq conn (pq-setdb pghost pgport pgoptions pgtty pgdbname))

;; check to see that the backend connection was successfully made
(when (= (pq-status conn) pg-connection-bad)
    (format t "Connection to database '~A' failed.~%" pgdbname)
    (format t "~A" (pq-error-message conn))
    (exit-nicely conn))

(setq res (pq-exec conn "LISTEN TBL2"))
(when (= (pq-status conn) pg-connection-bad)
    (format t "LISTEN command failed~%")
    (format t "~A" (pq-error-message conn))
    (exit-nicely conn))

;; Should PQclear PGresult whenever it is no longer needed to avoid memory leaks
(pq-clear res)

(loop
    ;; wait a little bit between checks; waiting with select()
    ;; would be more efficient.
	;; XXX Note: sleep not yet implemented in the lisp interpreter

    ;; collect any asynchronous backend messages
    (pq-consume-input conn)

    ;; check for asynchronous notify messages
    (when (setq notifies (pq-notifies conn))
	(format t "ASYNC NOTIFY of '~A' from backend pid '~D' received~%"
	 (pg-notify-relname notifies) (pg-notify-be-pid notifies))
    )
)

(pq-finish conn)
