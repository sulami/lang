;; This tests that we can declare a function and call it.

(defun double (x)
  (c/print_value "2 x ")
  (c/print_value x)
  (c/print_value " = ")
  (let ((result (* 2 x)))
    (c/print_value result)
    (c/print_value "\n")
    result))

(let ((result (double 21)))
  (c/print_value "Result was: ")
  (c/print_value result)
  (c/print_value "\n"))

(defun string-function (s)
  s)
(c/print_value (string-function "This is a very long string\n"))
