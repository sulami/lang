;; This tests that let bindings are working.

(let ((x 42))
  (c/print_int 42))

;; It also works in a nested fashion.

(let ((x 46)
      (y 42))
  (let ((x 42))
    (c/print_int x)
    (c/print_int y)))

;; It overrides global defs.

(def x 46)

(let ((x 42))
  (c/print_int 42))
