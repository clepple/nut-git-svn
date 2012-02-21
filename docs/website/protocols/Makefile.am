
SUFFIXES = .txt .html .pdf

all: doc

doc: @DOC_BUILD_LIST@

# Keep using 'HAVE_ASCIIDOC', instead of 'WITH_ASCIIDOC' since we have
# an explicit target
if HAVE_ASCIIDOC
website: html-single

else !HAVE_ASCIIDOC
website:
	@echo "Not building website since 'asciidoc' was not found."
endif !HAVE_ASCIIDOC

pdf: $(ASCIIDOC_PDF)
# also build the HTML manpages with these targets
html-single: $(ASCIIDOC_HTML_SINGLE)
html-chunked: $(ASCIIDOC_HTML_CHUNKED)

ASCIIDOC_HTML_SINGLE = apcsmart.html \
	belkin.html belkin-protocol/index.html \
	belkin-universal/index.html belkin-universal/old.html \
	eaton/index.html \
	eaton/fortress/page1.html eaton/fortress/page2.html \
	eaton/fortress/page3.html eaton/fortress/page4.html \
	eaton/fortress/page5.html eaton/fortress/page6.html \
	eaton/mge/index.html eaton/sola.html \
	megatec.html \
	minicol/index.html \
	oneac.html optiups.html \
	sec.html sec-1/index.html sec-1/page1.html \
	sec-1/page2.html sec-1/page3.html sec-1/page4.html \
	sec-1/page5.html sec-1/page6.html sec-1/page7.html \
	sec-1/page8.html sec-1/page9.html sec-1/page10.html \
	sec-1/page11.html sec-1/page12.html sec-1/page13.html \
	sec-1/page14.html sec-1/page15.html \
	snmp/index.html \
	us9003.html

ASCIIDOC_HTML_CHUNKED = eaton/fortress.chunked

# Note: without the "-v", asciidoc (circa 8.6.2) sometimes hangs when
# generating the chunked HTML. In this case, export the environment
# variable ASCIIDOC_VERBOSE to "-v", ie:
#   $ ASCIIDOC_VERBOSE=-v make
A2X_COMMON_OPTS = $(ASCIIDOC_VERBOSE) --attribute icons \
    --attribute localdate=`TZ=UTC date +%Y-%m-%d` \
    --attribute localtime=`TZ=UTC date +%H:%M:%S` \
    --attribute iconsdir=$(srcdir)/images \
    --attribute=badges \
    --attribute=external_title \
    --attribute tree_version=@TREE_VERSION@ \
    -a toc -a numbered --destination-dir=.

.txt.html:
	$(A2X) $(A2X_COMMON_OPTS) --attribute=xhtml11_format --format=xhtml $<

.txt.chunked:
	$(A2X) $(A2X_COMMON_OPTS) --attribute=chunked_format --format=chunked $<

.txt.pdf: docinfo.xml
	$(A2X) $(A2X_COMMON_OPTS) --attribute=pdf_format --format=pdf -a docinfo1 $<

.PHONY: html html-single pdf website

