; ModuleID = 'demo01.ll'
source_filename = "cfg_modify.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define i32 @foo(i32 %a) #0 {
entry:
  %add = add nsw i32 %a, 10
  %add1 = add nsw i32 %add, %a
  %add2 = add nsw i32 %a, %add1
  %cmp = icmp sgt i32 %add2, 100
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  br label %if.end.p1

if.else:                                          ; preds = %entry
  br label %if.end.p2

if.end.split3:                                    ; preds = %if.end.p2, %if.end.p1
  %.insert = phi i32 [ %z.p1, %if.end.p1 ], [ %z.p2, %if.end.p2 ]
  %.insert4 = phi i32 [ %w.p1, %if.end.p1 ], [ %w.p2, %if.end.p2 ]
  br label %if.end.split

if.end.split:                                     ; preds = %if.end.split3
  %r = add nsw i32 %.insert, %.insert4
  ret i32 %r

if.end.p2:                                        ; preds = %if.else
  %y.0.p21 = phi i32 [ 0, %if.else ]
  %z.p2 = add nsw i32 %y.0.p21, %a
  %w.p2 = add nsw i32 %y.0.p21, %add1
  br label %if.end.split3

if.end.p1:                                        ; preds = %if.then
  %y.0.p12 = phi i32 [ 10, %if.then ]
  %z.p1 = add nsw i32 %y.0.p12, %a
  %w.p1 = add nsw i32 %y.0.p12, %add1
  br label %if.end.split3
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.1 (tags/RELEASE_401/final)"}
