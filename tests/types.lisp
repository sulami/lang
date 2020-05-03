;; This tests out types as values.

(print_value (type 42))
(print_value "\n")

(print_value (type 3.14159))
(print_value "\n")

(print_value (type "foobar"))
(print_value "\n")

(print_value (type (type 42)))
(print_value "\n")

(print_value (type (type (type 42))))
(print_value "\n")

(let ((INT (type 42)))
  (print_value (value_equal INT
                            (type 46)))
  (print_value "\n"))
