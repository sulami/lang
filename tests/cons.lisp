;; This tests that cons cells work alright.

;; Print a whole cons cell.
(print_value (cons 42 nil))
(print_value "\n")

;; Extract car & cons.
(print_value (car (cons 42 nil)))
(print_value "\n")

(print_value (cdr (cons 42 nil)))
(print_value "\n")

;; Nested cons cell with varying types.
(print_value (cons "foo" (cons 42 nil)))
(print_value "\n")

(print_value (cons (cons "foo" nil) (cons 42 nil)))
(print_value "\n")
