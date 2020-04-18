;; This implements `map' in code. Tests first-class functions,
;; cons-lists, and recursion.

(defun nil? (x)
  (value_equal nil x))

(defun reverse (l)
  (let ((reverse* (lambda (from to)
                    (if (nil? from)
                        to
                        (recur (cdr from) (cons (car from) to))))))
    (reverse* l nil)))

(defun map (f l)
  (let ((map* (lambda (f from to)
                (if (nil? from)
                    to
                    (recur f
                           (cdr from)
                           (cons (f (car from)) to))))))
    (reverse (map* f l nil))))

(let ((list (cons 1 (cons 2 (cons 3 nil))))
      (double (lambda (x) (* 2 x))))
  (print_value double)
  (print_value "\n")
  (print_value (map double list))
  (print_value "\n"))
