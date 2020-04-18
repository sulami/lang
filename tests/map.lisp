;; This implements `map' in code. Tests first-class functions,
;; cons-lists, and recursion.

(defun nil? (x)
  (c/value_equal nil x))

(defun reverse (l)
  (let ((reverse* (lambda (from to)
                    (if (nil? from)
                        to
                        (recur (c/cdr from) (c/cons (c/car from) to))))))
    (reverse* l nil)))

(defun map (f l)
  (let ((map* (lambda (f from to)
                (if (nil? from)
                    to
                    (recur f
                           (c/cdr from)
                           (c/cons (f (c/car from)) to))))))
    (reverse (map* f l nil))))

(let ((list (c/cons 1 (c/cons 2 (c/cons 3 nil))))
      (double (lambda (x) (* 2 x))))
  (c/print_value double)
  (c/print_value "\n")
  (c/print_value (map double list))
  (c/print_value "\n"))
