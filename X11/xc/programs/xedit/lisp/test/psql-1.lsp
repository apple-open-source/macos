;; Postgresql C library interface, example program 1, using the xedit
;; lisp interface

;; Test the C version of libpq, the PostgreSQL frontend library. 
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
(setq pgdbname "template1")

;; make a connection to the database
(setq conn (pq-setdb pghost pgport pgoptions pgtty pgdbname))

;; check to see that the backend connection was successfully made
(when (= (pq-status conn) pg-connection-bad)
    (format t "Connection to database '~A' failed.~%" pgdbname)
    (format t "~A" (pq-error-message conn))
    (exit-nicely conn))

;; start a transaction block
(setq res (pq-exec conn "BEGIN"))
(when (or (null res) (not (= (pq-result-status res) pgres-command-ok)))
    (format t "BEGIN command failed~%")
    (pq-clear res)
    (exit-nicely conn))

;; Should PQclear PGresult whenever it is no longer needed to avoid memory leaks
(pq-clear res)

;; fetch rows from the pg_database, the system catalog of databases
(setq res (pq-exec conn "DECLARE mycursor CURSOR FOR select * from pg_database"))
(when (or (null res) (not (= (pq-result-status res) pgres-command-ok)))
    (format t "DECLARE CURSOR command failed~%")
    (pq-clear res)
    (exit-nicely conn))
(pq-clear res)
(setq res (pq-exec conn "FETCH ALL in mycursor"))
(when (or (null res) (not (= (pq-result-status res) pgres-tuples-ok)))
    (format t "FETCH ALL command didn't return tuples properly~%")
    (pq-clear res)
    (exit-nicely conn))

;; first, print out the attribute names
(setq nfields (pq-nfields res))
(dotimes (i nfields)
    (format t "~15@<~A~>" (pq-fname res i))
)
(format t "~%")

;; next, print out the rows
(setq ntuples (pq-ntuples res))
(dotimes (i ntuples)
    (dotimes (j nfields)
	(format t "~15@<~A~>" (pq-getvalue res i j))
    )
    (format t "~%")
)
(pq-clear res)

;; close the cursor
(setq res (pq-exec conn "CLOSE mycursor"))
(pq-clear res)

;; commit the transaction
(setq res (pq-exec conn "COMMIT"))
(pq-clear res)

;; close the connection to the database and cleanup
(pq-finish conn)
