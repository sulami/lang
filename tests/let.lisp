;; This tests that let bindings are working.

(let ((x 42))
  (print_value 42))
(print_value "\n")

;; It also works in a nested fashion.

(let ((x 46)
      (y 42))
  (let ((x 42))
    (print_value x)
    (print_value "\n")
    (print_value y)))
(print_value "\n")

;; It overrides global defs.

(def x 46)

(let ((x 42))
  (print_value 42))
(print_value "\n")

;; It returns its last value.

(print_value (let ((x 42))
                 (print_value x)
                 (print_value "\n")
                 x))
(print_value "\n")
