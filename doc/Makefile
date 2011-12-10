DOCBOOK ?= /usr/local/Cellar/docbook/5.0/docbook/xsl/1.75.2

all: manual.html manual.pdf

manual.html: manual.asciidoc
	asciidoc $<

manual.xml: manual.asciidoc
	asciidoc -b docbook -o $@ $<

manual.pdf: manual.xml
	xep -quiet -xml $< -xsl $(DOCBOOK)/fo/docbook.xsl -param paper.type=A4 -pdf $@

clean:
	rm -f manual.html manual.pdf manual.xml
