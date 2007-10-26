(when (cdr command-line-args)
  (setcdr command-line-args (cons "--no-splash" (cdr command-line-args))))
