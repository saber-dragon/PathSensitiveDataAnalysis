; ModuleID = 'cprop.cpp'
source_filename = "cprop.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define i32 @_Z3fooii(i32 %a, i32 %b) #0 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 %a, i32* %a.addr, align 4
  store i32 %b, i32* %b.addr, align 4
  store i32 2, i32* %y, align 4
  %0 = load i32, i32* %a.addr, align 4
  %cmp = icmp sgt i32 %0, 10
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i32 1, i32* %x, align 4
  br label %if.end8

if.else:                                          ; preds = %entry
  %1 = load i32, i32* %a.addr, align 4
  %cmp1 = icmp sgt i32 %1, 5
  br i1 %cmp1, label %if.then2, label %if.else3

if.then2:                                         ; preds = %if.else
  store i32 3, i32* %x, align 4
  br label %if.end7

if.else3:                                         ; preds = %if.else
  %2 = load i32, i32* %a.addr, align 4
  %cmp4 = icmp sgt i32 %2, 0
  br i1 %cmp4, label %if.then5, label %if.else6

if.then5:                                         ; preds = %if.else3
  store i32 100, i32* %x, align 4
  br label %if.end

if.else6:                                         ; preds = %if.else3
  %3 = load i32, i32* %a.addr, align 4
  store i32 %3, i32* %x, align 4
  br label %if.end

if.end:                                           ; preds = %if.else6, %if.then5
  br label %if.end7

if.end7:                                          ; preds = %if.end, %if.then2
  br label %if.end8

if.end8:                                          ; preds = %if.end7, %if.then
  br label %D

D:                                                ; preds = %if.end8
  %4 = load i32, i32* %x, align 4
  %cmp9 = icmp eq i32 %4, 8
  br i1 %cmp9, label %if.then10, label %if.else11

if.then10:                                        ; preds = %D
  %5 = load i32, i32* %a.addr, align 4
  store i32 %5, i32* %y, align 4
  br label %if.end16

if.else11:                                        ; preds = %D
  %6 = load i32, i32* %x, align 4
  %cmp12 = icmp eq i32 %6, 9
  br i1 %cmp12, label %if.then13, label %if.else14

if.then13:                                        ; preds = %if.else11
  %7 = load i32, i32* %b.addr, align 4
  store i32 %7, i32* %y, align 4
  br label %if.end15

if.else14:                                        ; preds = %if.else11
  %8 = load i32, i32* %a.addr, align 4
  %9 = load i32, i32* %b.addr, align 4
  %add = add nsw i32 %8, %9
  store i32 %add, i32* %y, align 4
  br label %if.end15

if.end15:                                         ; preds = %if.else14, %if.then13
  br label %if.end16

if.end16:                                         ; preds = %if.end15, %if.then10
  %10 = load i32, i32* %x, align 4
  store i32 %10, i32* %y, align 4
  %11 = load i32, i32* %y, align 4
  %12 = load i32, i32* %a.addr, align 4
  %cmp17 = icmp sgt i32 %11, %12
  br i1 %cmp17, label %if.then18, label %if.else19

if.then18:                                        ; preds = %if.end16
  %13 = load i32, i32* %b.addr, align 4
  store i32 %13, i32* %y, align 4
  br label %if.end20

if.else19:                                        ; preds = %if.end16
  %14 = load i32, i32* %a.addr, align 4
  store i32 %14, i32* %y, align 4
  br label %if.end20

if.end20:                                         ; preds = %if.else19, %if.then18
  %15 = load i32, i32* %x, align 4
  %16 = load i32, i32* %y, align 4
  %add21 = add nsw i32 %15, %16
  ret i32 %add21
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.1 (tags/RELEASE_401/final)"}
