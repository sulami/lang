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

(defun zero? (x)
  (= 0 x))

(defun pos? (x)
  (< 0 x))

(defun neg? (x)
  (< x 0))

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
  (if (or (= 0 i)
          (nil? l))
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

;; Chars

(defun whitespace? (c)
  (or (= c \space)
      (or (= c \tab)
          (= c \newline))))

;; Strings

(defun concat (x y)
  (concat_strings x y))

(defun str->cons (s)
  (string_to_cons s))

(defun cons->str (l)
  (cons_to_string l))

;; The actual compiler.

(defun parse (unparsed ast in-word?)
  (let ((c (car unparsed))
        (current-word (car ast))
        (rest (cdr ast)))

    (if (nil? c)
        ;; We're done parsing.
        (cons unparsed
              (cons (reverse ast)
                             nil))

        (if (whitespace? c)
            ;; Word ended, finalise last word.
            (recur (drop-while (lambda (c) (whitespace? c)) unparsed)
                   (cons (if (= (type \c)
                                (type (car current-word)))
                             (cons->str (reverse current-word))
                             (reverse current-word))
                         rest)
                   false)

            (if (= \( c)
                ;; Inner sexp, call down one level.
                (let ((inner (parse (cdr unparsed)
                                    nil
                                    false)))
                  (let ((inner-unparsed (car inner))
                        (inner-ast (cadr inner)))
                    (recur inner-unparsed
                           (cons (cons inner-ast nil)
                                 ast)
                           false)))

                (if (= \) c)
                    ;; Done in this sexp, return up one level.
                    (let ((final-ast
                           (cons (reverse (cons (cons->str (reverse current-word))
                                                rest))
                                 nil)))
                      (cons (cdr unparsed)
                            final-ast))

                    (if in-word?
                        ;; Already in a word, append to current word.
                        (recur (cdr unparsed)
                               (cons (cons c current-word)
                                     rest)
                               true)

                        ;; Otherwise start a new word.
                        (recur (cdr unparsed)
                               (cons (cons c nil)
                                     ast)
                               true))))))))

(defun compile (args)
  (let ((source-file (nth args 1)))
    (print "Compiling ")
    (print source-file)
    (println "...")
    (let ((source-code (str->cons (slurp source-file))))
      (println (cadr (parse source-code nil false))))))

(compile argv)

;; (defun parse (state)
;;   (let ((c (car (aget state "unparsed"))))
;;     (if (nil? c)
;;         state
;;         (if (= \( c)
;;             (let ((inner-state (parse state)))
;;               (recur
;;                (alist "ast" (lambda (x) ) inner-state)))
;;             (if (= \) c)
;;                 4
;;                 (if (whitespace? c)
;;                     5
;;                     (if (aget state "inside-word?")
;;                         6
;;                         7)))))))

;; -(defn parse [state]
;; -  (if-let [c (first (:unparsed state))]
;; -    (cond
;; -      (= \( c)
;; -      (let [inner-state (parse (-> state
;; -                                   (update :depth inc)
;; -                                   (update :unparsed rest)
;; -                                   (assoc :ast [])))]
;; -        (recur
;; -         (update inner-state :ast #(conj (:ast state) %))))
;; -
;; -      (= \) c)
;; -      (-> state
;; -          (update :depth dec)
;; -          (assoc :inside-word? false)
;; -          (update :unparsed rest))
;; -
;; -      (Character/isWhitespace c)
;; -      (recur
;; -       (-> state
;; -           (assoc :inside-word? false)
;; -           (update :unparsed rest)))
;; -
;; -      :else
;; -      (recur
;; -       (if (:inside-word? state)
;; -         (-> state
;; -             (update-in [:ast (-> state :ast count dec)] str c)
;; -             (update :unparsed rest))
;; -         (-> state
;; -             (update :ast #(conj % (str c)))
;; -             (assoc :inside-word? true)
;; -             (update :unparsed rest)))))
;; -
;; -    state))
