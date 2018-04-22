; ModuleID = 'nonssa_demo03.ll'
source_filename = "demo03.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define i32 @foo(i32 %x) #0 {
entry:
  br label %A

A:                                                ; preds = %entry
  %a = add i32 %x, 10
  br label %B

B:                                                ; preds = %A, %E
  %a.1 = phi i32 [ %a, %A ], [ %a.0, %E ]
  %b = add nsw i32 %a.1, 0
  %tobool = icmp ne i32 %x, 0
  br i1 %tobool, label %C, label %D

C:                                                ; preds = %B

  br label %E

D:                                                ; preds = %C, %B
  br label %E

E:                                                ; preds = %C, %D
  %a.0 = phi i32 [ 1, %C ], [ 2, %D ]
  %cmp = icmp eq i32 %a.0, 2
  br i1 %cmp, label %B, label %F

F:                                                ; preds = %E
  ret i32 %x
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.1 (tags/RELEASE_401/final)"}
