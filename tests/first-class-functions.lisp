;; This tests first class functions.

(def bar
    (lambda foo (x) (* x 2)))

(c/print_value (bar 21))
(c/print_value "\n")
