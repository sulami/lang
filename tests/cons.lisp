;; This tests that cons cells work alright.

;; Print a whole cons cell.
(c/print_value (c/cons 42 nil))
(c/print_value "\n")

;; Extract car & cons.
(c/print_value (c/car (c/cons 42 nil)))
(c/print_value "\n")

(c/print_value (c/cdr (c/cons 42 nil)))
(c/print_value "\n")

;; Nested cons cell with varying types.
(c/print_value (c/cons "foo" (c/cons 42 nil)))
(c/print_value "\n")

(c/print_value (c/cons (c/cons "foo" nil) (c/cons 42 nil)))
(c/print_value "\n")
