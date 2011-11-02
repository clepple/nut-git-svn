EXTRA_DIST = code_conformation.py

check_SCRIPTS = code_conformation.py

bin_SCRIPTS = code_conformation.py

check-local:
	@if python -c "import codecs,re,sys,os,glob,math"; then \
                echo "Executing Code Conformation script"; \
                python $(srcdir)/code_conformation.py; \
        else \
                echo "Python is not available."; \
        fi

