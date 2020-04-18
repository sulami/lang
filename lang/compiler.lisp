;; This is the compiler, bootstrapped.

;; At the same time this is also the ultimate test, as it's the most
;; complex program written in this language.

;; Convenience functions

(defun print (x)
  (print_value x))

(defun println (x)
  (print x)
  (print "\n"))

(defun slurp (file-name mode)
  (read_file file-name mode))

(defun cadr (x)
  (car (cdr x)))

(defun = (x y)
  (value_equal x y))

(defun nil? (x)
  (= nil x))

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

;; The actual compiler.

(defun compile (args)
  (let ((source-file (cadr args)))
    (print "Compiling ")
    (print source-file)
    (println "...")
    (println (slurp source-file "r"))))

(compile argv)
