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

;; Values

(defun not (x)
  (if x false true))

(defun and (x y)
  (if x y false))

(defun or (x y)
  (if x true y))

(defun = (x y)
  (value_equal x y))

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
                        (recur (cdr from) (cons (car from) to))))))
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

(defun take (l n)
  (let ((take* (lambda (l n acc)
                 (if (nil? l)
                     acc
                     (recur (cdr l)
                            (dec n)
                            (cons (car l) acc))))))
    (reverse (take* l n nil))))

(defun drop (l n)
  (if (= 0 n)
      l
      (recur (cdr l) (dec n))))

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
        (reduce* (lambda (f l acc)
                   (if (nil? l)
                       acc
                       (recur f
                              (cdr l)
                              (f acc (car l)))))))
    (reduce* f remaining-list first-result)))

;; The actual compiler.

(defun compile (args)
  (let ((source-file (cadr args)))
    (print "Compiling ")
    (print source-file)
    (println "...")
    (println (slurp source-file))))

(compile argv)
