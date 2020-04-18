;; Here we reverse a list, all in user code.

;; This is a good litmus test for memory management across function
;; boundaries.

(defun nil? (x)
  (value_equal nil x))

(defun reverse* (from to)
  (if (nil? from)
      to
      (recur (cdr from) (cons (car from) to))))

(defun reverse (l)
  (reverse* l nil))

(def start (cons 1 (cons 2 (cons 3 nil))))
(def reversed (reverse start))
(def reversed-reversed (reverse reversed))

(print_value start)
(print_value "\n")
(print_value reversed)
(print_value "\n")
(print_value reversed-reversed)
(print_value "\n")
(print_value (value_equal start reversed-reversed))
(print_value "\n")
