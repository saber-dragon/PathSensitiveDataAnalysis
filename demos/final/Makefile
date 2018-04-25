OPT=opt
RM=rm -rf



all: demo01.png demo01_modified.png demo02.png demo02_modified.png demo03.png demo03_modified.png


%_modified.png: %_modified.ll
	$(OPT) -dot-cfg-only $<
	dot -Tpng -o $@ `ls |grep *foo*.dot`
	$(RM) *.dot

%.png: %.ll
	$(OPT) -dot-cfg-only $<
	dot -Tpng -o $@ `ls |grep *foo*.dot`
	$(RM) *.dot

%_modified.ll: %.ll
	$(OPT) -S -load ./`ls |grep *.so` -`ls |grep -Po 'lib\K[^.]+'` $< -o $@

%.bc: %.ll 
	llvm-as $<

%.o: %.bc 
	llc -filetype=obj $<

demo01: demo01.o test.o 
	clang++ -O0 demo01.o test.o -o demo01

clean:
	$(RM) *_modified.ll *.dot *.png *.o




