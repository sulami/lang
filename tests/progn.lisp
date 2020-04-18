;; This tests that progn works.

;; It runs several operations.
(progn
  (print_value "This is ")
  (print_value "several values.\n"))

;; It returns the values of its last expression.

(print_value (progn
                 46
                 42))
(print_value "\n")

;; Empty progn returns nil

(print_value (progn))
(print_value "\n")
