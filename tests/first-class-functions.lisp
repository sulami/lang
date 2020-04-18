;; This tests first class functions.

;; Explicit assignment
(def bar
    (lambda foo (x) (* x 2)))

(c/print_value (bar 21))
(c/print_value "\n")

;; Defun should also add them to the global namespace.
(defun baz (x)
  (+ 3 x))

(defun print-function (x)
  (c/print_value x))

(print-function baz)
(c/print_value "\n")

;; We should be able to pass them somewhere and then call them, and
;; pass back their return value.
(defun apply-function (f x)
  (f x))

(c/print_value (apply-function baz 39))
(c/print_value "\n")
