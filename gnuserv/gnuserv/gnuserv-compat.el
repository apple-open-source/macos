;; gnuserv-compat.el - Help GNU XEmacs gnuserv.el work under GNU Emacs.
;; Copyright (C) 1998, 1999, 2000 Martin Schwenke
;;
;; Author: Martin Schwenke <martin@meltin.net>
;; Maintainer: Martin Schwenke <martin@meltin.net>
;; Created: 20 November 1998
;; $Id$
;; Keywords: gnuserv

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
;; GNU General Public License for more details.
;;
;; If you have not received a copy of the GNU General Public License
;; along with this software, it can be obtained from the GNU Project's
;; World Wide Web server (http://www.gnu.org/copyleft/gpl.html), from
;; its FTP server (ftp://ftp.gnu.org/pub/gnu/GPL), by sending an electronic
;; mail to this program's maintainer or by writing to the Free Software
;; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

;;; Commentary:
;;
;; Under non-XEmacs (tested 19.34 <= ... <= 20.7)
;;
;;   (autoload 'gnuserv-start "gnuserv-compat"
;;             "Allow this Emacs process to be a server for client processes."
;;             t)
;;
;; Note that this file does a (require 'gnuserv) near the end.
;;
;; This code does a few things including:
;;
;; * A poor emulation of XEmacs' device handling, mapping devices to
;;   frames.  See the (tiny bit of) code for details.  Note that this
;;   emulation might only work for the version of gnuserv that it
;;   comes with.  Other stuff that uses XEmacs devices might behave
;;   badly when used with this emulation.


;;; Code:

;; Miscellaneous functions that are in XEmacs but not GNU Emacs up to
;; 20.3.  Also, XEmacs preloads the common lisp stuff, and we might as
;; well use it here.

(require 'cl)

(unless (fboundp 'define-obsolete-variable-alias)
  (defalias 'define-obsolete-variable-alias 'make-obsolete-variable))

(unless (fboundp 'functionp)
  (defun functionp (object)
    "Non-nil if OBJECT is a type of object that can be called as a function."
    (or (subrp object) (byte-code-function-p object)
	(eq (car-safe object) 'lambda)
	(and (symbolp object) (fboundp object)))))

;;; temporary-file-directory not available in 19.34
(unless (boundp 'temporary-file-directory)
  (defvar temporary-file-directory
    (cond
     ((getenv "TMPDIR"))
     (t "/tmp"))))

(unless (fboundp 'temp-directory)
  (defun temp-directory ()
    "Return the pathname to the directory to use for temporary files.
On NT/MSDOS, this is obtained from the TEMP or TMP environment variables,
defaulting to the value of `temporary-file-directory' if they are both
undefined.  On Unix it is obtained from TMPDIR, with the value of
`temporary-file-directory' as the default."

    (if	(eq system-type 'windows-nt)
	(cond
	 ((getenv "TEMP"))
	 ((getenv "TMP"))
	 (t (directory-file-name temporary-file-directory)))
      (cond
	  ((getenv "TMPDIR"))
	  (t (directory-file-name temporary-file-directory))))))


;; If we're not running XEmacs then advise `make-frame',
;; `delete-frame' and `filtered-frame-list' to handle some device
;; stuff.

(if (string-match "XEmacs" (emacs-version))
    nil
  
  ;; XEmacs `make-frame' takes an optional device to create the frame
  ;; on.  Since `make-device' just calls 'make-frame', we don't want
  ;; to make a new frame on both occasions.  Therefore, if the device
  ;; already represents a live frame, we modify the frame parameters
  ;; as desired and then return the existing frame.  Modifying the
  ;; frame parameters can cause an annoying flicker, but that's all we
  ;; can do!  If the device doesn't represent a live frame, we create
  ;; the frame as requested.

  (defadvice make-frame (around
			 gnuserv-compat-make-frame
			 first
			 (&optional parameters device)
			 activate)
    (if (and device
	     (frame-live-p device))
	(progn
	  (if parameters
	      (modify-frame-parameters device parameters))
	  (setq ad-return-value device))
      ad-do-it))

  ;; Advise `delete-frame' to run `delete-device-hook'.  This might be a
  ;; little too hacky, but it seems to work!  If someone actually tries
  ;; to do something device specific then it will probably blow up!
  (defadvice delete-frame (before
			   gnuserv-compat-delete-frame
			   first
			   nil
			   activate)
    (run-hook-with-args 'delete-device-hook frame))

  ;; Advise `filtered-frame-list' to ignore the optional device
  ;; argument.  Here we don't follow the mapping of devices to frames.
  ;; We just assume that any frame satisfying the predicate will do.
  (defadvice filtered-frame-list (around
				  gnuserv-compat-filtered-frame-list
				  first
				  (predicate &optional device)
				  activate)
    ad-do-it))


;; Emulate XEmacs devices.  A device is just a frame. For the most
;; part we use devices.el from the Emacs-W3 distribution.  In some
;; places the implementation seems wrong, so we "fix" it!

(if (string-match "XEmacs" (emacs-version))
    nil

  (require 'devices)
  (defalias 'device-list 'frame-list)
  (defalias 'selected-device 'selected-frame)
  (defun device-frame-list (&optional device)
    (list 
     (if device
	device
       (selected-frame)))))
  


;; Check iconification and perform deiconification the GNU Emacs way.
;; There might be some XEmacs subtlty that I'm missing, but it seems
;; to do the job.
(unless (fboundp 'frame-iconified-p)
  (defun frame-iconified-p (frame)
    (equal (frame-visible-p frame) 'icon)))

(unless (fboundp 'deiconify-frame)
  (defalias 'deiconify-frame 'make-frame-visible))

;; GNU Emacs doesn't have a way of checking if a frame is totally
;; visible, so we just do something sensible.
(unless (fboundp 'frame-totally-visible-p)
  (defun frame-totally-visible-p (frame)
    (eq t (frame-visible-p frame))))

;; Make custom stuff work even without customize
;;   Courtesy of Hrvoje Niksic <hniksic@srce.hr>
;;   via Ronan Waide <waider@scope.ie>.
(eval-and-compile
  (condition-case ()
      (require 'custom)
    (error nil))
  (if (and (featurep 'custom) (fboundp 'custom-declare-variable))
      nil ;; We've got what we needed
    ;; We have the old custom-library, hack around it!
    (defmacro defgroup (&rest args)
      nil)
    (defmacro defcustom (var value doc &rest args) 
      (` (defvar (, var) (, value) (, doc))))
    (defmacro defface (var value doc &rest args)
      (` (make-face (, var))))
    (defmacro define-widget (&rest args)
      nil)))

;; Now for gnuserv...
(require 'gnuserv)

(provide 'gnuserv-compat)

;;; gnuserv-compat.el ends here
