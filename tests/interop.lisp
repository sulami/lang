;; This tests that we can call C functions with native, boxed values.
;; The functions referenced here are declared with their desired
;; types, which helps casting.

(c/print_int 42)

(c/print_bool false)

(c/print_int (+ 2 3))
