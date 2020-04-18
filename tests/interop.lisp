;; This tests that we can call C functions with native, boxed values.
;; The functions referenced here are declared with their desired
;; types, which helps casting.

(print_value false)
(print_value "\n")

(print_value 42)
(print_value "\n")

(print_value 3.14159)
(print_value "\n")

(print_value (+ 38 4))
(print_value "\n")

(print_value "a string\n")
