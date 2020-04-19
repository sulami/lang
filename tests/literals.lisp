;; This tests that literals are read & printed correctly.

(print_value 42)
(print_value "\n")
(print_value -42)
(print_value "\n")

(print_value 3.14159)
(print_value "\n")
(print_value -3.14159)
(print_value "\n")

(print_value true)
(print_value "\n")
(print_value false)
(print_value "\n")
(print_value nil)
(print_value "\n")

(print_value "a string")
(print_value "\n")

(print_value (cons 42 (cons "a string" nil)))
(print_value "\n")

(print_value \s)
(print_value "\n")
