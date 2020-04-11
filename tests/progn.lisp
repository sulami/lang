;; This tests that progn works.

;; It runs several operations.
(progn
  (c/print_value "This is ")
  (c/print_value "several values.\n"))

;; It returns the values of its last expression.

(c/print_value (progn
                 46
                 42))
(c/print_value "\n")

;; Empty progn returns nil

(c/print_value (progn))
(c/print_value "\n")
