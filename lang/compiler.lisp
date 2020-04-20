;; This is the compiler, bootstrapped.

;; At the same time this is also the ultimate test, as it's the most
;; complex program written in this language.

;; Convenience functions

;; I/O

(defun print (x)
  (print_value x))

(defun println (x)
  (print x)
  (print "\n"))

(defun slurp (file-name)
  (read_file file-name "r"))

(defun spit (file-name s)
  (write_file file-name "w" s))

;; Logic

(defun not (x)
  (if x false true))

(defun and (x y)
  (if x y false))

(defun or (x y)
  (if x true y))

(defun xor (x y)
  (if x y (not y)))

(defun if-not (condition then else)
  (if (not condition)
      then
      else))

;; Values

(defun = (x y)
  (value_equal x y))

(defun not= (x y)
  (not (= x y)))

(defun nil? (x)
  (= nil x))

(defun inc (x)
  (+ x 1))

(defun dec (x)
  (- x 1))

;; Lists

(defun cadr (x)
  (car (cdr x)))

(defun reverse (l)
  (let ((reverse* (lambda (from to)
                    (if (nil? from)
                        to
                        (recur (cdr from)
                               (cons (car from) to))))))
    (reverse* l nil)))

(defun count (l)
  (let ((count* (lambda (l n)
                  (if (nil? l)
                      n
                      (recur (cdr l) (inc n))))))
    (count* l 0)))

(defun last (l)
  (if (nil? (cdr l))
      (car l)
      (recur (cdr l))))

(defun nth (l i)
  (if (= 0 i)
      (car l)
      (recur (cdr l) (dec i))))

(defun take (n l)
  (let ((take* (lambda (n from to)
                 (if (or (nil? from)
                         (= 0 n))
                     to
                     (recur (dec n)
                            (cdr from)
                            (cons (car from) to))))))
    (reverse (take* n l nil))))

(defun drop (n l)
  (if (= 0 n)
      l
      (recur (dec n) (cdr l))))

(defun take-while (f l)
  (let ((take-while* (lambda (f from to)
                       (if (or (nil? from)
                               (not (f (car from))))
                           to
                           (recur f
                                  (cdr from)
                                  (cons (car from) to))))))
    (reverse (take-while* f l nil))))

(defun drop-while (f l)
  (if (or (nil? l)
          (not (f (car l))))
      l
      (recur f (cdr l))))

(defun filter (f l)
  (let ((filter* (lambda (f from to)
                   (if (nil? from)
                       to
                       (let ((this (car from)))
                         (recur f
                                (cdr from)
                                (if (f this)
                                    (cons this to)
                                    to)))))))
    (reverse (filter* f l nil))))

(defun map (f l)
  (let ((map* (lambda (f from to)
                (if (nil? from)
                    to
                    (recur f
                           (cdr from)
                           (cons (f (car from)) to))))))
    (reverse (map* f l nil))))

(defun reduce (f l)
  (let ((first-result (f (car l) (car (cdr l))))
        (remaining-list (cdr (cdr l)))
        (reduce* (lambda (f from to)
                   (if (nil? from)
                       to
                       (recur f
                              (cdr from)
                              (f to (car from)))))))
    (reduce* f remaining-list first-result)))

;; Strings

(defun concat (x y)
  (concat_strings x y))

(defun str->cons (s)
  (string_to_cons s))

(defun cons->str (l)
  (cons_to_string l))

;; The actual compiler.

(defun compile (args)
  (let ((source-file (cadr args)))
    (print "Compiling ")
    (print source-file)
    (println "...")
    (let ((source-code (slurp source-file))
          (target-file "output"))
      (println (cons->str (str->cons source-code)))
      ;; (println (concat "this is " "a string"))
      ;; (println source-code)
      ;; (spit target-file source-code)
      (println "Done!"))))

(compile argv)
