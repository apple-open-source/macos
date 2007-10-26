;; Disable menu bar except for GUI emacs
(if (not window-system)
  (add-hook 'term-setup-hook #'(lambda () (menu-bar-mode -1))))

(custom-set-variables
 '(gud-gdb-command-name "gdb --annotate=1")
 '(large-file-warning-threshold nil)
)
