;; Postgresql C library interface, example program 3, using the xedit
;; lisp interface

;;  Test the binary cursor interface
;;
;; populate a database by doing the following:
;;
;; CREATE TABLE test1 (i int4, d real, p polygon);
;;
;; INSERT INTO test1 values (1, 3.567, polygon '(3.0, 4.0, 1.0, 2.0)');
;;
;; INSERT INTO test1 values (2, 89.05, polygon '(4.0, 3.0, 2.0, 1.0)');
;;
;; the expected output is:
;;
;; tuple 0: got i = (4 bytes) 1, d = (4 bytes) 3.567000, p = (4
;; bytes) 2 points   boundbox = (hi=3.000000/4.000000, lo =
;; 1.000000,2.000000) tuple 1: got i = (4 bytes) 2, d = (4 bytes)
;; 89.050003, p = (4 bytes) 2 points   boundbox =
;; (hi=4.000000/3.000000, lo = 2.000000,1.000000)

;;  Output of the lisp code:
;;
;; type[0] = 23, size[0] = 4
;; type[1] = 700, size[1] = 4
;; type[2] = 604, size[2] = -1
;; tuple 0: got
;;  i = (4 bytes) 1
;;  d = (4 bytes) 3.567
;;  p = (4 bytes) 2 points boundbox = (hi=3.0/4.0, lo = 1.0/2.0)
;; tuple 1: got
;;  i = (4 bytes) 2
;;  d = (4 bytes) 89.05
;;  p = (4 bytes) 2 points boundbox = (hi=4.0/3.0, lo = 2.0/1.0)


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

(setq res (pq-exec conn "BEGIN"))
(when (= (pq-status conn) pg-connection-bad)
    (format t "BEGIN command failed~%")
    (pq-clear res)
    (exit-nicely conn))

;; Should PQclear PGresult whenever it is no longer needed to avoid memory leaks
(pq-clear res)

(setq res (pq-exec conn "DECLARE mycursor BINARY CURSOR FOR select * from test1"))
(when (= (pq-status conn) pg-connection-bad)
    (format t "DECLARE CURSOR command failed~%")
    (pq-clear res)
    (exit-nicely conn))
(pq-clear res)

(setq res (pq-exec conn "FETCH ALL in mycursor"))
(when (or (null res) (not (= (pq-result-status res) pgres-tuples-ok)))
    (format t "FETCH ALL command didn't return tuples properly~%")
    (pq-clear res)
    (exit-nicely conn))

(setq i-fnum (pq-fnumber res "i"))
(setq d-fnum (pq-fnumber res "d"))
(setq p-fnum (pq-fnumber res "p"))

(dotimes (i 3)
    (format t "type[~D] = ~D, size[~D] = ~D~%"
     i (pq-ftype res i) i (pq-fsize res i))
)

(dotimes (i (pq-ntuples res))
    (setq i-val (pq-getvalue res i i-fnum 'int32))
    (setq d-val (pq-getvalue res i d-fnum 'float))
    (setq p-val (pq-getvalue res i p-fnum 'pg-polygon))
    (format t "tuple ~D: got~%" i)
    (format t " i = (~D bytes) ~D~%" (pq-getlength res i i-fnum) i-val)
    (format t " d = (~D bytes) ~D~%" (pq-getlength res i d-fnum) d-val)
    (format t " p = (~D bytes) ~D points~,8@Tboundbox = (hi=~F/~F, lo = ~F/~F)~%"
     (pq-getlength res i d-fnum) (pg-polygon-num-points p-val)
     (pg-point-x (pg-box-high (pg-polygon-boundbox p-val)))
     (pg-point-y (pg-box-high (pg-polygon-boundbox p-val)))
     (pg-point-x (pg-box-low (pg-polygon-boundbox p-val)))
     (pg-point-y (pg-box-low (pg-polygon-boundbox p-val))))
)
(pq-clear res)

(setq res (pq-exec conn "CLOSE mycursor"))
(pq-clear res)

(setq res (pq-exec conn "COMMIT"))
(pq-clear res)

(pq-finish conn)
