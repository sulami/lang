;; This tests that we can do simple recursion.

(defun count (i)
  (print_value "counting: ")
  (print_value i)
  (print_value "\n")
  (if (< i 10)
      (recur (+ i 1))
      i))

(print_value (count 0))
(print_value "\n")

;; This tests that we can recurse without blowing the stack.

(defun countmore (i)
  (if (< i 1000000)
      (recur (+ 1 i))
      i))

(print_value (countmore 2))
(print_value "\n")

;; A bit more sophistication, and complex arguments.

(defun powers-of-two (acc limit)
  (let ((next (* 2 (car acc))))
    (if (< next limit)
        (recur (cons next acc) limit)
        (cons next acc))))

(print_value (powers-of-two (cons 2 nil) 1024))
(print_value "\n")
