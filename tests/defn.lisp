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

;; defun also binds a function value into global scope

(defun print-a-function ()
  (print_value string-function))

(print-a-function)
(print_value "\n")

;; defuns can call themselves without TCO

(defun count (x)
  (if (value_equal 0 x)
      0
      (progn
        (print_value x)
        (print_value "\n")
        (count (- x 1)))))

(count 5)

;; functions can call other functions with functions as arguments

(defun f () 42)
(defun g (x) (x))
(print_value (g f))
(print_value "\n")
