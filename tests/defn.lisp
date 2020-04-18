;; This tests that we can declare a function and call it.

(defun double (x)
  (print_value "2 x ")
  (print_value x)
  (print_value " = ")
  (let ((result (* 2 x)))
    (print_value result)
    (print_value "\n")
    result))

(let ((result (double 21)))
  (print_value "Result was: ")
  (print_value result)
  (print_value "\n"))

(defun string-function (s)
  s)
(print_value (string-function "This is a very long string\n"))
