;; This implements `map' in code. Tests first-class functions,
;; cons-lists, and recursion.

;; Auxiliary functions

(defun nil? (x)
  (c/value_equal nil x))

(defun reverse* (from to)
  (if (nil? from)
      to
      (recur (c/cdr from) (c/cons (c/car from) to))))

(defun reverse (l)
  (reverse* l nil))

(defun map* (f xs acc)
  (if (nil? xs)
      acc
      (recur f
             (c/cdr xs)
             (c/cons (f (c/car xs)) acc))))

(defun map (f l)
  (reverse (map* f l nil)))

(let ((list (c/cons 1 (c/cons 2 (c/cons 3 nil))))
      (double (lambda (x) (* 2 x))))
  (c/print_value double)
  (c/print_value "\n")
  (c/print_value (map double list))
  (c/print_value "\n"))
