; ModuleID = 'modified_demo03.ll'
source_filename = "demo03.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define i32 @foo(i32 %x) #0 {
entry:
  br label %A

A:                                                ; preds = %entry
  %a = add i32 %x, 10
  br label %B.p0

E.F_crit_edge:                                    ; preds = %E.p2, %E.p1
  br label %F

F:                                                ; preds = %E.F_crit_edge
  ret i32 %x

B.p0:                                             ; preds = %A
  %a.1.p0.p0 = phi i32 [ %a, %A ]
  %b.p0 = add nsw i32 %a.1.p0.p0, 0
  %tobool.p0 = icmp ne i32 %x, 0
  br i1 %tobool.p0, label %C.p0, label %D.p0

C.p0:                                             ; preds = %B.p0
  br label %E.p1

D.p0:                                             ; preds = %B.p0
  br label %E.p2

E.p1:                                             ; preds = %C.p2, %C.p1, %C.p0
  br i1 false, label %B.p1, label %E.F_crit_edge

E.p2:                                             ; preds = %D.p2, %D.p1, %D.p0
  br i1 true, label %B.p2, label %E.F_crit_edge

B.p1:                                             ; preds = %E.p1
  br i1 undef, label %C.p1, label %D.p1

B.p2:                                             ; preds = %E.p2
  %tobool.p2 = icmp ne i32 %x, 0
  br i1 %tobool.p2, label %C.p2, label %D.p2

C.p1:                                             ; preds = %B.p1
  br label %E.p1

D.p1:                                             ; preds = %B.p1
  br label %E.p2

C.p2:                                             ; preds = %B.p2
  br label %E.p1

D.p2:                                             ; preds = %B.p2
  br label %E.p2
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.1 (tags/RELEASE_401/final)"}
