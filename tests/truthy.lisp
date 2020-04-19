;; This tests that literal values can be truthy.

;; Integers
(print_value (if 1 42 46))
(print_value "\n")
(print_value (if 0 46 42))
(print_value "\n")
(print_value (if -1 46 42))
(print_value "\n")

;; Floats
(print_value (if 1.4 42 46))
(print_value "\n")
(print_value (if 0.0 46 42))
(print_value "\n")
(print_value (if -1.4 46 42))
(print_value "\n")

;; Bools & nil
(print_value (if true 42 46))
(print_value "\n")
(print_value (if false 46 42))
(print_value "\n")
(print_value (if nil 46 42))
(print_value "\n")

;; Strings, Cons
(print_value (if "a" 42 46))
(print_value "\n")
(print_value (if "" 46 42))
(print_value "\n")
(print_value (if (cons 0 nil) 42 46))
(print_value "\n")
