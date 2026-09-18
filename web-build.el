;; Builds the documentation

(require 'package)
(require 'package-vc)
(add-to-list 'package-archives
	     '("melpa" . "https://melpa.org/packages/") t)

(package-initialize)
(package-refresh-contents)

(unless (package-installed-p 'htmlize)
  (package-install 'htmlize))

(require 'ox-publish)
(require 'htmlize)
(require 'ob-dot)

(setq org-html-htmlize-output-type 'css)
(setq org-html-htmlize-font-prefix "org-src-")

(setq org-confirm-babel-evaluate
      (lambda (lang body)
        (not (member lang '("shell" "dot" "emacs-lisp")))))

(org-babel-do-load-languages
 'org-babel-load-languages
 '((dot . t)
   (emacs-lisp . t)
   (shell . t)))

(defvar my/project-root (expand-file-name default-directory))

(setq org-publish-project-alist
      '(("docs-pages"
         :base-directory "./doc/"
         :base-extension "org"
         :publishing-directory "./web-doc/"
         :with-html-preamble t
         :html-preamble (lambda (options)
                          (with-temp-buffer
                            (insert-file-contents (expand-file-name "doc/preamble.html" my/project-root))
                            (buffer-string)))
         :recursive t
         :publishing-function org-html-publish-to-html)

        ("html-pages"
	     :base-directory "./doc/"
	     :base-extension "css\\|html"
	     :publishing-directory "./web-doc/"
	     :recursive t
	     :publishing-function org-publish-attachment)))

(org-publish-all t)
