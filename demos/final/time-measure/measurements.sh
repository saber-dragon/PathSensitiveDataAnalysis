#!/bin/bash

function ApplyPathSensitiveAnalysisOn(){
    In=$1
    Lib=$2
    PN=$3
    Out=$4
    echo "Apply path-sensitive analysis on ${In} ..."
    opt -S -load ./${Lib} -${PN} ${In} -o ${Out}
}

function ApplySCCPOn(){
    In=$1
    Out=${In%%.ll}.bc
    echo "Apply SCCP on ${In} ..."
    opt -sccp ${In} -o ${Out}
}

function ApplySCCPOnLLV(){
    In=$1
    Out=sccp_${In%%.ll}.ll
    echo "Apply SCCP on ${In} ..."
    opt -S -sccp ${In} -o ${Out}
}

function BC2Binary(){
    In=$1
    Out=$2
    Inter=${In%%.bc}.o
    echo "Generate ${Inter} from ${In} ..."
    llc -filetype=obj ${In} -o ${Inter}
    echo "Generate ${Out} ..."
    clang++ -O0 ${Inter} test.o -o ${Out}
    rm ${Inter}
}



# for file in *.ll; do 
#     ApplyPathSensitiveAnalysisOn ${file} `ls |grep *.so` `ls |grep -Po 'lib\K[^.]+'` modified_${file} >/dev/null 2>&1
# done 

# for file in *.ll; do
#     ApplySCCPOn ${file}
# done 

# for file in *.bc; do 
#     BC2Binary ${file} ${file%%.*}
# done 
for file in *.ll; do
    ApplySCCPOnLLV ${file}
done 

    
    