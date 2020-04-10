;; This tests that we can call C functions with native, boxed values.
;; The functions referenced here are declared with their desired
;; types, which helps casting.

(c/print_value 42)

(c/print_value false)

(c/print_value (+ 2 3))
