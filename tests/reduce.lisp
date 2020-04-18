;; This implements `reduce' in code. Tests first-class functions,
;; cons-lists, and recursion.

(defun nil? (x)
  (value_equal nil x))

(defun reduce (f l)
  (let ((first-result (f (car l) (car (cdr l))))
        (remaining-list (cdr (cdr l)))
        (reduce* (lambda (f l acc)
                   (if (nil? l)
                       acc
                       (recur f
                              (cdr l)
                              (f acc (car l)))))))
    (reduce* f remaining-list first-result)))

(let ((list (cons 1 (cons 2 (cons 3 nil)))))
  (print_value (reduce (lambda (x y) (+ x y)) list))
  (print_value "\n"))
