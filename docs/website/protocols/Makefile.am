
SUFFIXES = .txt .html .pdf

all: doc

doc: @DOC_BUILD_LIST@

pdf: $(ASCIIDOC_PDF)
# also build the HTML manpages with these targets
html-single: $(ASCIIDOC_HTML_SINGLE)
html-chunked: $(ASCIIDOC_HTML_CHUNKED)

ASCIIDOC_HTML_SINGLE = apcsmart.html fortress/index.html \
	belkin.html megatec.html sec.html \
	oneac.html optiups.html us9003.html \
	snmp/index.html belkin-protocol/index.html \
	belkin-universal/index.html belkin-universal/old.html \
	eaton/index.html eaton/sola.html eaton/fortress/page1.html \
	eaton/fortress/page2.html eaton/fortress/page3.html eaton/fortress/page4.html \
	eaton/fortress/page5.html eaton/fortress/page6.html eaton/mge/index.html \
	minicol/index.html sec-1/index.html sec-1/page1.html\
	sec-1/page2.html sec-1/page3.html sec-1/page4.html \
	sec-1/page5.html sec-1/page6.html sec-1/page7.html \
	sec-1/page8.html sec-1/page9.html sec-1/page10.html \
	sec-1/page11.html sec-1/page12.html sec-1/page13.html \
	sec-1/page14.html sec-1/page15.html

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

apcsmart.html: apcsmart.txt

eaton/fortress.chunked: eaton/fortress.txt

eaton/fortress/index.html: eaton/fortress/index.txt

belkin.html: belkin.txt

megatec.html: megatec.txt

sec.html: sec.txt

oneac.html: oneac.txt

optiups.html: optiups.txt

us9003.html: us9003.txt

snmp/index.html: snmp/index.txt

belkin-protocol/index.html: belkin-protocol/index.txt

belkin-universal/index.html: belkin-universal/index.txt

belkin-universal/old.html: belkin-universal/old.txt

eaton/index.html: eaton/index.txt

eaton/sola.html: eaton/sola.txt

eaton/fortress/page1.html: eaton/fortress/page1.txt

eaton/fortress/page2.html: eaton/fortress/page2.txt

eaton/fortress/page3.html: eaton/fortress/page3.txt

eaton/fortress/page4.html: eaton/fortress/page4.txt

eaton/fortress/page5.html: eaton/fortress/page5.txt

eaton/fortress/page6.html: eaton/fortress/page6.txt

eaton/mge/index.html: eaton/mge/index.txt

minicol/index.html: minicol/index.txt

sec-1/index.html: sec-1/index.txt

sec-1/page1.html: sec-1/page1.txt

sec-1/page2.html: sec-1/page2.txt

sec-1/page3.html: sec-1/page3.txt

sec-1/page4.html: sec-1/page4.txt

sec-1/page5.html: sec-1/page5.txt

sec-1/page6.html: sec-1/page6.txt

sec-1/page7.html: sec-1/page7.txt

sec-1/page8.html: sec-1/page8.txt

sec-1/page9.html: sec-1/page9.txt

sec-1/page10.html: sec-1/page10.txt

sec-1/page11.html: sec-1/page11.txt

sec-1/page12.html: sec-1/page12.txt

sec-1/page13.html: sec-1/page13.txt

sec-1/page14.html: sec-1/page14.txt

sec-1/page15.html: sec-1/page15.txt

.txt.html:
	$(A2X) $(A2X_COMMON_OPTS) --attribute=xhtml11_format --format=xhtml $<

.txt.chunked:
	$(A2X) $(A2X_COMMON_OPTS) --attribute=chunked_format --format=chunked $<

.txt.pdf: docinfo.xml
	$(A2X) $(A2X_COMMON_OPTS) --attribute=pdf_format --format=pdf -a docinfo1 $<

.PHONY: html html-single pdf website

