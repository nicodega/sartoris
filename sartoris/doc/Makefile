all : isma.pdf isma.ps sartoris.ps sma.pdf

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
	
sma.pdf : sma.dvi
	dvipdfm sma.dvi

sma.ps : sma.dvi
	dvips sma.dvi -o

sma.dvi : sma.tex
	latex sma.tex
	latex sma.tex

.PHONY : clean
clean :
	rm -f sma.log
	rm -f sma.aux
	rm -f sma.dvi
	rm -f sma.ps
	rm -f sma.toc
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
