;; Here we reverse a list, all in user code.

;; This is a good litmus test for memory management across function
;; boundaries.

(defun nil? (x)
  (c/value_equal nil x))

(defun reverse* (from to)
  (if (nil? from)
      to
      (recur (c/cdr from) (c/cons (c/car from) to))))

(defun reverse (l)
  (reverse* l nil))

(def start (c/cons 1 (c/cons 2 (c/cons 3 nil))))
(def reversed (reverse start))
(def reversed-reversed (reverse reversed))

(c/print_value start)
(c/print_value "\n")
(c/print_value reversed)
(c/print_value "\n")
(c/print_value reversed-reversed)
(c/print_value "\n")
(c/print_value (c/value_equal start reversed-reversed))
(c/print_value "\n")
