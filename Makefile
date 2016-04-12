OUT = htm
IN = bib
all: $(OUT).pdf

$(OUT).pdf: $(OUT).tex
	pdflatex $(OUT)
	bibtex $(OUT)
	pdflatex $(OUT)
	pdflatex $(OUT)

	
clean:
	rm -f *~ htm.aux htm.pdf htm.brf htm.bbl htm.blg htm.log htm.out
