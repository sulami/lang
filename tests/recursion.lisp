;; This tests that we can do simple recursion.

(defn count (i)
  (c/print_value "counting: ")
  (c/print_value i)
  (c/print_value "\n")
  (if (< i 10)
      (count (+ 1 i))
      i))

(c/print_value (count 0))
(c/print_value "\n")

;; This tests that we can recurse without blowing the stack.

(defn countmore (i)
  (if (< i 1000000)
      (recur (+ 1 i))
      i))

(c/print_value (countmore 2))
(c/print_value "\n")

;; A bit more sophistication, and complex arguments.

(defn powers-of-two (acc limit)
  (let ((next (* 2 (c/car acc))))
    (if (< next limit)
        (recur (c/cons next acc) limit)
        (c/cons next acc))))

(c/print_value (powers-of-two (c/cons 2 nil) 1024))
(c/print_value "\n")
