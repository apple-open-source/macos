;;; gnuserv.el --- Lisp interface code between Emacs and gnuserv
;; Copyright (C) 1989-1997 Free Software Foundation, Inc.

;; Version: 3.12
;; Author: Andy Norman (ange@hplb.hpl.hp.com), originally based on server.el
;;         Hrvoje Niksic <hniksic@xemacs.org>
;; Maintainer: Jan Vroonhof <vroonhof@math.ethz.ch>,
;;             Hrvoje Niksic <hniksic@xemacs.org>
;; Keywords: environment, processes, terminals

;; This file is part of XEmacs.

;; XEmacs is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; XEmacs is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with XEmacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Synched up with: Not in FSF.

;;; Commentary:

;; Gnuserv is run when Emacs needs to operate as a server for other
;; processes.  Specifically, any number of files can be attached for
;; editing to a running XEmacs process using the `gnuclient' program.

;; Use `M-x gnuserv-start' to start the server and `gnuclient files'
;; to load them to XEmacs.  When you are done with a buffer, press
;; `C-x #' (`M-x gnuserv-edit').  You can put (gnuserv-start) to your
;; .emacs, and enable `gnuclient' as your Unix "editor".  When all the
;; buffers for a client have been edited and exited with
;; `gnuserv-edit', the client "editor" will return to the program that
;; invoked it.

;; Your editing commands and Emacs' display output go to and from the
;; terminal or X display in the usual way.  If you are running under
;; X, a new X frame will be open for each gnuclient.  If you are on a
;; TTY, this TTY will be attached as a new device to the running
;; XEmacs, and will be removed once you are done with the buffer.

;; To evaluate a Lisp form in a running Emacs, use the `-eval'
;; argument of gnuclient.  To simplify this, we provide the `gnudoit'
;; shell script.  For example `gnudoit "(+ 2 3)"' will print `5',
;; whereas `gnudoit "(gnus)"' will fire up your favorite newsreader.
;; Like gnuclient, `gnudoit' requires the server to be started prior
;; to using it.

;; For more information you can refer to man pages of gnuclient,
;; gnudoit and gnuserv, distributed with XEmacs.

;; gnuserv.el was originally written by Andy Norman as an improvement
;; over William Sommerfeld's server.el.  Since then, a number of
;; people have worked on it, including Bob Weiner, Darell Kindred,
;; Arup Mukherjee, Ben Wing and Jan Vroonhof.  It was completely
;; rewritten (labeled as version 3) by Hrvoje Niksic in May 1997.  The
;; new code will not run on GNU Emacs.

;; Jan Vroonhof <vroonhof@math.ethz.ch> July/1996
;; ported the server-temp-file-regexp feature from server.el
;; ported server hooks from server.el
;; ported kill-*-query functions from server.el (and made it optional)
;; synced other behavior with server.el
;;
;; Jan Vroonhof
;;     Customized.
;;
;; Hrvoje Niksic <hniksic@xemacs.org> May/1997
;;     Completely rewritten.  Now uses `defstruct' and other CL stuff
;;     to define clients cleanly.  Many thanks to Dave Gillespie!
;;
;; Mike Scheidler <c23mts@eng.delcoelect.com> July, 1997
;;     Added 'Done' button to the menubar.


;;; Code:

(defgroup gnuserv nil
  "The gnuserv suite of programs to talk to Emacs from outside."
  :group 'environment
  :group 'processes
  :group 'terminals)


;; Provide the old variables as aliases, to avoid breaking .emacs
;; files.  However, they are obsolete and should be converted to the
;; new forms.  This ugly crock must be before the variable
;; declaration, or the scheme fails.

(define-obsolete-variable-alias 'server-frame 'gnuserv-frame)
(define-obsolete-variable-alias 'server-done-function
  'gnuserv-done-function)
(define-obsolete-variable-alias 'server-done-temp-file-function
  'gnuserv-done-temp-file-function)
(define-obsolete-variable-alias 'server-find-file-function
  'gnuserv-find-file-function)
(define-obsolete-variable-alias 'server-program
  'gnuserv-program)
(define-obsolete-variable-alias 'server-visit-hook
  'gnuserv-visit-hook)
(define-obsolete-variable-alias 'server-done-hook
  'gnuserv-done-hook)
(define-obsolete-variable-alias 'server-kill-quietly
  'gnuserv-kill-quietly)
(define-obsolete-variable-alias 'server-temp-file-regexp
  'gnuserv-temp-file-regexp)
(define-obsolete-variable-alias 'server-make-temp-file-backup
  'gnuserv-make-temp-file-backup)

;;;###autoload
(defcustom gnuserv-frame nil
  "*The frame to be used to display all edited files.
If nil, then a new frame is created for each file edited.
If t, then the currently selected frame will be used.
If a function, then this will be called with a symbol `x' or `tty' as the
only argument, and its return value will be interpreted as above."
  :tag "Gnuserv Frame"
  :type '(radio (const :tag "Create new frame each time" nil)
		(const :tag "Use selected frame" t)
	        (function-item :tag "Use main Emacs frame"
			       gnuserv-main-frame-function)
		(function-item :tag "Use visible frame, otherwise create new"
			       gnuserv-visible-frame-function)
		(function-item :tag "Create special Gnuserv frame and use it"
			       gnuserv-special-frame-function)
		(function :tag "Other"))
  :group 'gnuserv
  :group 'frames)

(defcustom gnuserv-frame-plist nil
  "*Plist of frame properties for creating a gnuserv frame."
  :type 'plist
  :group 'gnuserv
  :group 'frames)

(defcustom gnuserv-done-function 'kill-buffer
  "*Function used to remove a buffer after editing.
It is called with one BUFFER argument.  Functions such as `kill-buffer' and
`bury-buffer' are good values. See also `gnuserv-done-temp-file-function'."
  :type '(radio (function-item kill-buffer)
		(function-item bury-buffer)
		(function :tag "Other"))
  :group 'gnuserv)

(defcustom gnuserv-done-temp-file-function 'kill-buffer
  "*Function used to remove a temporary buffer after editing.
It is called with one BUFFER argument.  Functions such as `kill-buffer' and
`bury-buffer' are good values. See also `gnuserv-done-temp-file-function'."
  :type '(radio (function-item kill-buffer)
		(function-item bury-buffer)
		(function :tag "Other"))
  :group 'gnuserv)

(defcustom gnuserv-find-file-function 'find-file
  "*Function to visit a file with.
It takes one argument, a file name to visit."
  :type 'function
  :group 'gnuserv)

(defcustom gnuserv-view-file-function 'view-file
  "*Function to view a file with.
It takes one argument, a file name to view."
  :type '(radio (function-item view-file)
		(function-item find-file-read-only)
		(function :tag "Other"))
  :group 'gnuserv)

(defcustom gnuserv-program "gnuserv"
  "*Program to use as the editing server."
  :type 'string
  :group 'gnuserv)

(defcustom gnuserv-visit-hook nil
  "*Hook run after visiting a file."
  :type 'hook
  :group 'gnuserv)

(defcustom gnuserv-done-hook nil
  "*Hook run when done editing a buffer for the Emacs server.
The hook functions are called after the file has been visited, with the
current buffer set to the visiting buffer."
  :type 'hook
  :group 'gnuserv)

(defcustom gnuserv-init-hook nil
  "*Hook run after the server is started."
  :type 'hook
  :group 'gnuserv)

(defcustom gnuserv-shutdown-hook nil
  "*Hook run before the server exits."
  :type 'hook
  :group 'gnuserv)

(defcustom gnuserv-kill-quietly nil
  "*Non-nil means to kill buffers with clients attached without requiring confirmation."
  :type 'boolean
  :group 'gnuserv)

(defcustom gnuserv-temp-file-regexp
  (concat "^" (temp-directory) "/Re\\|/draft$")
  "*Regexp which should match filenames of temporary files deleted
and reused by the programs that invoke the Emacs server."
  :type 'regexp
  :group 'gnuserv)

(defcustom gnuserv-make-temp-file-backup nil
  "*Non-nil makes the server backup temporary files also."
  :type 'boolean
  :group 'gnuserv)


;;; Internal variables:

(defstruct gnuclient
  "An object that encompasses several buffers in one.
Normally, a client connecting to Emacs will be assigned an id, and
will request editing of several files.

ID       - Client id (integer).
BUFFERS  - List of buffers that \"belong\" to the client.
           NOTE: one buffer can belong to several clients.
DEVICE   - The device this client is on.  If the device was also created.
           by a client, it will be placed to `gnuserv-devices' list.
FRAME    - Frame created by the client, or nil if the client didn't
           create a frame.

All the slots default to nil."
  (id nil)
  (buffers nil)
  (device nil)
  (frame nil))

(defvar gnuserv-process nil
  "The current gnuserv process.")

(defvar gnuserv-string ""
  "The last input string from the server.")

(defvar gnuserv-current-client nil
  "The client we are currently talking to.")

(defvar gnuserv-clients nil
  "List of current gnuserv clients.
Each element is a gnuclient structure that identifies a client.")

(defvar gnuserv-devices nil
  "List of devices created by clients.")

(defvar gnuserv-special-frame nil
  "Frame created specially for Server.")

;; We want the client-infested buffers to have some modeline
;; identification, so we'll make a "minor mode".
(defvar gnuserv-minor-mode nil)
(make-variable-buffer-local 'gnuserv-mode)
(pushnew '(gnuserv-minor-mode " Server") minor-mode-alist
	  :test 'equal)


;; Sample gnuserv-frame functions

(defun gnuserv-main-frame-function (type)
  "Return a sensible value for the main Emacs frame."
  (if (or (eq type 'x)
	  (eq type 'mswindows))
      (car (frame-list))
    nil))

(defun gnuserv-visible-frame-function (type)
  "Return a frame if there is a frame that is truly visible, nil otherwise.
This is meant in the X sense, so it will not return frames that are on another
visual screen.  Totally visible frames are preferred.  If none found, return nil."
  (if (or (eq type 'x)
	  (eq type 'mswindows))
      (cond ((car (filtered-frame-list 'frame-totally-visible-p
				       (selected-device))))
	    ((car (filtered-frame-list (lambda (frame)
					 ;; eq t as in not 'hidden
					 (eq t (frame-visible-p frame)))
				       (selected-device)))))
    nil))

(defun gnuserv-special-frame-function (type)
  "Create a special frame for Gnuserv and return it on later invocations."
  (unless (frame-live-p gnuserv-special-frame)
    (setq gnuserv-special-frame (make-frame gnuserv-frame-plist)))
  gnuserv-special-frame)


;;; Communication functions

;; We used to restart the server here, but it's too risky -- if
;; something goes awry, it's too easy to wind up in a loop.
(defun gnuserv-sentinel (proc msg)
  (let ((msgstring (concat "Gnuserv process %s; restart with `%s'"))
	(keystring (substitute-command-keys "\\[gnuserv-start]")))
  (case (process-status proc)
    (exit
     (message msgstring "exited" keystring)
     (gnuserv-prepare-shutdown))
    (signal
     (message msgstring "killed" keystring)
     (gnuserv-prepare-shutdown))
    (closed
     (message msgstring "closed" keystring))
     (gnuserv-prepare-shutdown))))

;; This function reads client requests from our current server.  Every
;; client is identified by a unique ID within the server
;; (incidentally, the same ID is the file descriptor the server uses
;; to communicate to client).
;;
;; The request string can arrive in several chunks.  As the request
;; ends with \C-d, we check for that character at the end of string.
;; If not found, keep reading, and concatenating to former strings.
;; So, if at first read we receive "5 (gn", that text will be stored
;; to gnuserv-string.  If we then receive "us)\C-d", the two will be
;; concatenated, `current-client' will be set to 5, and `(gnus)' form
;; will be evaluated.
;;
;; Server will send the following:
;;
;; "ID <text>\C-d"  (no quotes)
;;
;;  ID    - file descriptor of the given client;
;; <text> - the actual contents of the request.
(defun gnuserv-process-filter (proc string)
  "Process gnuserv client requests to execute Emacs commands."
  (setq gnuserv-string (concat gnuserv-string string))
  ;; C-d means end of request.
  (when (string-match "\C-d\n?\\'" gnuserv-string)
    (cond ((string-match "\\`[0-9]+" gnuserv-string) ; client request id
	   (let ((header (read-from-string gnuserv-string)))
	     ;; Set the client we are talking to.
	     (setq gnuserv-current-client (car header))
	     ;; Evaluate the expression
	     (condition-case oops
		 (eval (car (read-from-string gnuserv-string (cdr header))))
	       ;; In case of an error, write the description to the
	       ;; client, and then signal it.
	       (error (setq gnuserv-string "")
		      (when gnuserv-current-client
			(gnuserv-write-to-client gnuserv-current-client oops))
		      (setq gnuserv-current-client nil)
		      (signal (car oops) (cdr oops)))
	       (quit (setq gnuserv-string "")
		     (when gnuserv-current-client
		       (gnuserv-write-to-client gnuserv-current-client oops))
		     (setq gnuserv-current-client nil)
		     (signal 'quit nil)))
	     (setq gnuserv-string "")))
	  (t
	   (let ((response (car (split-string gnuserv-string "\C-d"))))
	     (setq gnuserv-string "")
	     (error "%s: invalid response from gnuserv" response))))))

;; This function is somewhat of a misnomer.  Actually, we write to the
;; server (using `process-send-string' to gnuserv-process), which
;; interprets what we say and forwards it to the client.  The
;; incantation server understands is (from gnuserv.c):
;;
;; "FD/LEN:<text>\n"  (no quotes)
;;    FD     - file descriptor of the given client (which we obtained from
;;             the server earlier);
;;    LEN    - length of the stuff we are about to send;
;;    <text> - the actual contents of the request.
(defun gnuserv-write-to-client (client-id form)
  "Write the given form to the given client via the gnuserv process."
  (when (eq (process-status gnuserv-process) 'run)
    (let* ((result (format "%s" form))
	   (s      (format "%s/%d:%s\n" client-id
			   (length result) result)))
      (process-send-string gnuserv-process s))))

;; The following two functions are helper functions, used by
;; gnuclient.

(defun gnuserv-eval (form)
  "Evaluate form and return result to client."
  (gnuserv-write-to-client gnuserv-current-client (eval form))
  (setq gnuserv-current-client nil))

(defun gnuserv-eval-quickly (form)
  "Let client know that we've received the request, and then eval the form.
This order is important as not to keep the client waiting."
  (gnuserv-write-to-client gnuserv-current-client nil)
  (setq gnuserv-current-client nil)
  (eval form))


;; "Execute" a client connection, called by gnuclient.  This is the
;; backbone of gnuserv.el.
(defun gnuserv-edit-files (type list &rest flags)
  "For each (line-number . file) pair in LIST, edit the file at line-number.
The visited buffers are memorized, so that when \\[gnuserv-edit] is invoked
in such a buffer, or when it is killed, or the client's device deleted, the
client will be invoked that the edit is finished.

TYPE should either be a (tty TERM) list, or (x DISPLAY) list.
If a flag is `quick', just edit the files in Emacs.
If a flag is `view', view the files read-only."
  (let (quick view)
    (mapc (lambda (flag)
	    (case flag
	      (quick (setq quick t))
	      (view  (setq view t))
	      (t     (error "Invalid flag %s" flag))))
	  flags)
    (let* ((old-device-num (length (device-list)))
	   (new-frame nil)
	   (dest-frame (if (functionp gnuserv-frame)
			   (funcall gnuserv-frame (car type))
			 gnuserv-frame))
	   ;; The gnuserv-frame dependencies are ugly, but it's
	   ;; extremely hard to make that stuff cleaner without
	   ;; breaking everything in sight.
	   (device (cond ((frame-live-p dest-frame)
			  (frame-device dest-frame))
			 ((null dest-frame)
			  (case (car type)
			    (tty tty (cdr type))
			    (x   (make-x-device (cadr type)))
			    (mswindows   (make-mswindows-device))
			    (t   (error "Invalid device type"))))
			 (t
			  (selected-device))))
	   (frame (cond ((frame-live-p dest-frame)
			 dest-frame)
			((null dest-frame)
			 (setq new-frame (make-frame gnuserv-frame-plist
						     device))
			 new-frame)
			(t (selected-frame))))
	   (client (make-gnuclient :id gnuserv-current-client
				   :device device
				   :frame new-frame)))
      (select-frame frame)
      (setq gnuserv-current-client nil)
      ;; If the device was created by this client, push it to the list.
      (and (/= old-device-num (length (device-list)))
	   (push device gnuserv-devices))
      (and (frame-iconified-p frame)
	   (deiconify-frame frame))
      ;; Visit all the listed files.
      (while list
	(let ((line (caar list)) (path (cdar list)))
	  (select-frame frame)
	  ;; Visit the file.
	  (funcall (if view
		       gnuserv-view-file-function
		     gnuserv-find-file-function)
		   path)
	  (goto-line line)
	  ;; Don't memorize the quick and view buffers.
	  (unless (or quick view)
	    (pushnew (current-buffer) (gnuclient-buffers client))
	    (setq gnuserv-minor-mode t)
	    ;; Add the "Done" button to the menubar, only in this buffer.
	    (if (and (featurep 'menubar) current-menubar)
	      (progn (set-buffer-menubar current-menubar)
	      (add-menu-button nil ["Done" gnuserv-edit]))
	      ))
	  (run-hooks 'gnuserv-visit-hook)
	  (pop list)))
      (cond
       ((or quick view)
	;; Exit if quick or view.  NOTE: if the
	;; client is to finish now, it must absolutely /not/ be
	;; included to the list of clients.  This way the client-ids
	;; should be unique.
	(gnuserv-write-to-client (gnuclient-id client) nil))
       (t
	;; Else, the client gets a vote.
	(push client gnuserv-clients)
	;; Explain buffer exit options.  If dest-frame is nil, the
	;; user can exit via `delete-frame'.  OTOH, if FLAGS are nil
	;; and there are some buffers, the user can exit via
	;; `gnuserv-edit'.
	(if (and (not (or quick view))
		 (gnuclient-buffers client))
	    (message "%s"
		     (substitute-command-keys
		      "Type `\\[gnuserv-edit]' to finish editing"))
	  (or dest-frame
	      (message "%s"
		       (substitute-command-keys
			"Type `\\[delete-frame]' to finish editing")))))))))


;;; Functions that hook into Emacs in various way to enable operation

;; Defined later.
(add-hook 'kill-emacs-hook 'gnuserv-kill-all-clients t)

;; A helper function; used by others.  Try avoiding it whenever
;; possible, because it is slow, and conses a list.  Use
;; `gnuserv-buffer-p' when appropriate, for instance.
(defun gnuserv-buffer-clients (buffer)
  "Return a list of clients to which BUFFER belongs."
  (let (res)
    (dolist (client gnuserv-clients)
      (when (memq buffer (gnuclient-buffers client))
	(push client res)))
    res))

;; Like `gnuserv-buffer-clients', but returns a boolean; doesn't
;; collect a list.
(defun gnuserv-buffer-p (buffer)
  (member* buffer gnuserv-clients
	   :test 'memq
	   :key 'gnuclient-buffers))

;; This function makes sure that a killed buffer is deleted off the
;; list for the particular client.
;;
;; This hooks into `kill-buffer-hook'.  It is *not* a replacement for
;; `kill-buffer' (thanks God).
(defun gnuserv-kill-buffer-function ()
  "Remove the buffer from the buffer lists of all the clients it belongs to.
Any client that remains \"empty\" after the removal is informed that the
editing has ended."
  (let* ((buf (current-buffer)))
    (dolist (client (gnuserv-buffer-clients buf))
      (callf2 delq buf (gnuclient-buffers client))
      ;; If no more buffers, kill the client.
      (when (null (gnuclient-buffers client))
	(gnuserv-kill-client client)))))

(add-hook 'kill-buffer-hook 'gnuserv-kill-buffer-function)

;; Ask for confirmation before killing a buffer that belongs to a
;; living client.
(defun gnuserv-kill-buffer-query-function ()
  (or gnuserv-kill-quietly
      (not (gnuserv-buffer-p (current-buffer)))
      (yes-or-no-p
       (format "Buffer %s belongs to gnuserv client(s); kill anyway? "
	       (current-buffer)))))

(add-hook 'kill-buffer-query-functions
	  'gnuserv-kill-buffer-query-function)

(defun gnuserv-kill-emacs-query-function ()
  (or gnuserv-kill-quietly
      (not (some 'gnuclient-buffers gnuserv-clients))
      (yes-or-no-p "Gnuserv buffers still have clients; exit anyway? ")))

(add-hook 'kill-emacs-query-functions
	  'gnuserv-kill-emacs-query-function)

;; If the device of a client is to be deleted, the client should die
;; as well.  This is why we hook into `delete-device-hook'.
(defun gnuserv-check-device (device)
  (when (memq device gnuserv-devices)
    (dolist (client gnuserv-clients)
      (when (eq device (gnuclient-device client))
	;; we must make sure that the server kill doesn't result in
	;; killing the device, because it would cause a device-dead
	;; error when `delete-device' tries to do the job later.
	(gnuserv-kill-client client t))))
  (callf2 delq device gnuserv-devices))

(add-hook 'delete-device-hook 'gnuserv-check-device)

(defun gnuserv-temp-file-p (buffer)
  "Return non-nil if BUFFER contains a file considered temporary.
These are files whose names suggest they are repeatedly
reused to pass information to another program.

The variable `gnuserv-temp-file-regexp' controls which filenames
are considered temporary."
  (and (buffer-file-name buffer)
       (string-match gnuserv-temp-file-regexp (buffer-file-name buffer))))

(defun gnuserv-kill-client (client &optional leave-frame)
  "Kill the gnuclient CLIENT.
This will do away with all the associated buffers.  If LEAVE-FRAME,
the function will not remove the frames associated with the client."
  ;; Order is important: first delete client from gnuserv-clients, to
  ;; prevent gnuserv-buffer-done-1 calling us recursively.
  (callf2 delq client gnuserv-clients)
  ;; Process the buffers.
  (mapc 'gnuserv-buffer-done-1 (gnuclient-buffers client))
  (unless leave-frame
    (let ((device (gnuclient-device client)))
      ;; kill frame created by this client (if any), unless
      ;; specifically requested otherwise.
      ;;
      ;; note: last frame on a device will not be deleted here.
    (when (and (gnuclient-frame client)
	       (frame-live-p (gnuclient-frame client))
	       (second (device-frame-list device)))
      (delete-frame (gnuclient-frame client)))
    ;; If the device is live, created by a client, and no longer used
    ;; by any client, delete it.
    (when (and (device-live-p device)
	       (memq device gnuserv-devices)
	       (second (device-list))
	       (not (member* device gnuserv-clients
			     :key 'gnuclient-device)))
      ;; `gnuserv-check-device' will remove it from `gnuserv-devices'.
      (delete-device device))))
  ;; Notify the client.
  (gnuserv-write-to-client (gnuclient-id client) nil))

;; Do away with the buffer.
(defun gnuserv-buffer-done-1 (buffer)
  (dolist (client (gnuserv-buffer-clients buffer))
    (callf2 delq buffer (gnuclient-buffers client))
    (when (null (gnuclient-buffers client))
      (gnuserv-kill-client client)))
  ;; Get rid of the buffer.
  (save-excursion
    (set-buffer buffer)
    (run-hooks 'gnuserv-done-hook)
    (setq gnuserv-minor-mode nil)
    ;; Delete the menu button.
    (if (and (featurep 'menubar) current-menubar)
      (delete-menu-item '("Done")))
    (funcall (if (gnuserv-temp-file-p buffer)
		 gnuserv-done-temp-file-function
	       gnuserv-done-function)
	     buffer)))


;;; Higher-level functions

;; Choose a `next' server buffer, according to several criteria, and
;; return it.  If none are found, return nil.
(defun gnuserv-next-buffer ()
  (let* ((frame (selected-frame))
	 (device (selected-device))
	 client)
    (cond
     ;; If we have a client belonging to this frame, return
     ;; the first buffer from it.
     ((setq client
	    (car (member* frame gnuserv-clients :key 'gnuclient-frame)))
      (car (gnuclient-buffers client)))
     ;; Else, look for a device.
     ((and
       (memq (selected-device) gnuserv-devices)
       (setq client
	     (car (member* device gnuserv-clients :key 'gnuclient-device))))
      (car (gnuclient-buffers client)))
     ;; Else, try to find any client with at least one buffer, and
     ;; return its first buffer.
     ((setq client
	    (car (member-if-not #'null gnuserv-clients
				:key 'gnuclient-buffers)))
      (car (gnuclient-buffers client)))
     ;; Oh, give up.
     (t nil))))

(defun gnuserv-buffer-done (buffer)
  "Mark BUFFER as \"done\" for its client(s).
Does the save/backup queries first, and calls `gnuserv-done-function'."
  ;; Check whether this is the real thing.
  (unless (gnuserv-buffer-p buffer)
    (error "%s does not belong to a gnuserv client" buffer))
  ;; Backup/ask query.
  (if (gnuserv-temp-file-p buffer)
      ;; For a temp file, save, and do NOT make a non-numeric backup
      ;; Why does server.el explicitly back up temporary files?
      (let ((version-control nil)
	    (buffer-backed-up (not gnuserv-make-temp-file-backup)))
	(save-buffer))
    (if (and (buffer-modified-p)
	     (y-or-n-p (concat "Save file " buffer-file-name "? ")))
	(save-buffer buffer)))
  (gnuserv-buffer-done-1 buffer))

;; Called by `gnuserv-start-1' to clean everything.  Hooked into
;; `kill-emacs-hook', too.
(defun gnuserv-kill-all-clients ()
  "Kill all the gnuserv clients.  Ruthlessly."
  (mapc 'gnuserv-kill-client gnuserv-clients))

;; This serves to run the hook and reset
;; `allow-deletion-of-last-visible-frame'.
(defun gnuserv-prepare-shutdown ()
  (setq allow-deletion-of-last-visible-frame nil)
  (run-hooks 'gnuserv-shutdown-hook))

;; This is a user-callable function, too.
(defun gnuserv-shutdown ()
  "Shutdown the gnuserv server, if one is currently running.
All the clients will be disposed of via the normal methods."
  (interactive)
  (gnuserv-kill-all-clients)
  (when gnuserv-process
    (set-process-sentinel gnuserv-process nil)
    (gnuserv-prepare-shutdown)
    (condition-case ()
	(delete-process gnuserv-process)
      (error nil))
    (setq gnuserv-process nil)))

;; Actually start the process.  Kills all the clients before-hand.
(defun gnuserv-start-1 (&optional leave-dead)
  ;; Shutdown the existing server, if any.
  (gnuserv-shutdown)
  ;; If we already had a server, clear out associated status.
  (unless leave-dead
    (setq gnuserv-string ""
	  gnuserv-current-client nil)
    (let ((process-connection-type t))
      (setq gnuserv-process
	    (start-process "gnuserv" nil gnuserv-program)))
    (set-process-sentinel gnuserv-process 'gnuserv-sentinel)
    (set-process-filter gnuserv-process 'gnuserv-process-filter)
    (process-kill-without-query gnuserv-process)
    (setq allow-deletion-of-last-visible-frame t)
    (run-hooks 'gnuserv-init-hook)))


;;; User-callable functions:

;;;###autoload
(defun gnuserv-running-p ()
  "Return non-nil if a gnuserv process is running from this XEmacs session."
  (not (not gnuserv-process)))

;;;###autoload
(defun gnuserv-start (&optional leave-dead)
  "Allow this Emacs process to be a server for client processes.
This starts a gnuserv communications subprocess through which
client \"editors\" (gnuclient and gnudoit) can send editing commands to
this Emacs job.  See the gnuserv(1) manual page for more details.

Prefix arg means just kill any existing server communications subprocess."
  (interactive "P")
  (and gnuserv-process
       (not leave-dead)
       (message "Restarting gnuserv"))
  (gnuserv-start-1 leave-dead))

(defun gnuserv-edit (&optional count)
  "Mark the current gnuserv editing buffer as \"done\", and switch to next one.

Run with a numeric prefix argument, repeat the operation that number
of times.  If given a universal prefix argument, close all the buffers
of this buffer's clients.

The `gnuserv-done-function' (bound to `kill-buffer' by default) is
called to dispose of the buffer after marking it as done.

Files that match `gnuserv-temp-file-regexp' are considered temporary and
are saved unconditionally and backed up if `gnuserv-make-temp-file-backup'
is non-nil.  They are disposed of using `gnuserv-done-temp-file-function'
\(also bound to `kill-buffer' by default).

When all of a client's buffers are marked as \"done\", the client is notified."
  (interactive "P")
  (when (null count)
    (setq count 1))
  (cond ((numberp count)
	 (while (natnump (decf count))
	   (let ((frame (selected-frame)))
	     (gnuserv-buffer-done (current-buffer))
	     (when (eq frame (selected-frame))
	       ;; Switch to the next gnuserv buffer.  However, do this
	       ;; only if we remain in the same frame.
	       (let ((next (gnuserv-next-buffer)))
		 (when next
		   (switch-to-buffer next)))))))
	(count
	   (let* ((buf (current-buffer))
		  (clients (gnuserv-buffer-clients buf)))
	     (unless clients
	       (error "%s does not belong to a gnuserv client" buf))
	     (mapc 'gnuserv-kill-client (gnuserv-buffer-clients buf))))))

(global-set-key "\C-x#" 'gnuserv-edit)

(provide 'gnuserv)

;;; gnuserv.el ends here
