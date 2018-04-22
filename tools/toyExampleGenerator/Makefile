CC=clang
CXX=clang++
CXXFLAGS=-S -emit-llvm -O0
RM=rm -rf
CP=cp -r


CC_FILES=$(wildcard *.cpp)
LL_OBJS=$(subst .cpp,.ll,$(CC_FILES))
PNG_OBJS=$(subst .cpp,.png,$(CC_FILES))

all: $(PNG_OBJS) $(LL_OBJS)

%.png: %.ll
	opt -dot-cfg-only $< >/dev/null
	dot -Tpng -o $@ `ls |grep *foo*.dot`
	$(RM) *.dot


%.ll: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o nonssa_$@
	echo "Convert to SSA form ..."
	opt -S -mem2reg nonssa_$@ -o $@


install:
	$(CP) *.ll ../../tests

clean:
	$(RM) *.dot *.ll *.png