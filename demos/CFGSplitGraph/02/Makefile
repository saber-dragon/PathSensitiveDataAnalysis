OPT=opt
RM=rm -rf
obj=cfg02_modified.png
src=cfg02_before_modify.ll
intermediate=cfg01_modified.ll
byproduct=cfg01_before_modify.png

all: $(obj) $(byproduct)

$(obj): $(intermediate)
	$(OPT) -dot-cfg-only $<
	dot -Tpng -o $@ `ls |grep *foo*.dot`
	$(RM) *.dot

$(intermediate): $(src)
	$(OPT) -S -load ./`ls |grep *.so` -Temp $< -o $@

$(byproduct): $(src)
	$(OPT) -dot-cfg-only $<
	dot -Tpng -o $@ `ls |grep *foo*.dot`
	$(RM) *.dot

clean:
	$(RM) cfg01_modified.ll *.dot *.png 




