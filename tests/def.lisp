;; This tests that def bindings are working.

(defun f (x)
  (print_value x))

(def v 42)

(f v)
(print_value "\n")
