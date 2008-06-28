all : isma.pdf isma.ps sartoris.ps

sartoris.ps : sartoris.dvi
	dvips sartoris.dvi -o

sartoris.dvi : sartoris.tex
	latex sartoris.tex
	latex sartoris.tex

isma.pdf : isma.dvi
	dvipdfm isma.dvi

isma.ps : isma.dvi
	dvips isma.dvi -o

isma.dvi : isma.tex
	latex isma.tex
	latex isma.tex

.PHONY : clean
clean :
	rm -f isma.log
	rm -f isma.aux
	rm -f isma.dvi
	rm -f isma.ps
	rm -f isma.toc
	rm -f sartoris.log
	rm -f sartoris.aux
	rm -f sartoris.dvi
	rm -f sartoris.ps
	rm -f sartoris.toc
	rm -f *~

clean_all : clean
