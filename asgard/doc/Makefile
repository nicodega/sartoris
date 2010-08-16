all : ofs.ps ofs.pdf asgard.ps asgard.pdf

ofs.pdf : ofs.dvi
    dvipdfm ofs.dvi

ofs.ps : ofs.dvi
    dvips ofs.dvi -o

ofs.dvi : ofs.tex
    latex ofs.tex && latex ofs.tex

asgard.pdf : asgard.dvi
    dvipdfm asgard.dvi

asgard.ps : asgard.dvi
    dvips asgard.dvi -o

asgard.dvi : asgard.tex
    latex asgard.tex && latex asgard.tex

.PHONY : clean
clean :
    rm -f ofs.log
    rm -f ofs.aux
    rm -f ofs.dvi
    rm -f ofs.ps
    rm -f ofs.toc
    rm -f ofs.pdf
    rm -f asgard.log
    rm -f asgard.aux
    rm -f asgard.dvi
    rm -f asgard.ps
    rm -f asgard.toc
    rm -f asgard.pdf
    rm -f *~

clean_all : clean
