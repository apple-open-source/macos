;; gnuserv setup.
(autoload 'gnuserv-start "gnuserv-compat"
  "Allow this Emacs process to be a server for client processes."
  t)

;; Use the "open" program for URL browsing.
(setq browse-url-browser-function (quote browse-url-generic))
(setq browse-url-generic-program "/usr/bin/open")
