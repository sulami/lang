;; This tests that let bindings are working.

(let ((x 42))
  (c/print_value 42))
(c/print_value "\n")

;; It also works in a nested fashion.

(let ((x 46)
      (y 42))
  (let ((x 42))
    (c/print_value x)
    (c/print_value "\n")
    (c/print_value y)))
(c/print_value "\n")

;; It overrides global defs.

(def x 46)

(let ((x 42))
  (c/print_value 42))
(c/print_value "\n")

;; It returns its last value.

(c/print_value (let ((x 42))
                 (c/print_value x)
                 (c/print_value "\n")
                 x))
(c/print_value "\n")
