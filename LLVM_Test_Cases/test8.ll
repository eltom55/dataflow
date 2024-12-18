
define i32 @main() {
entry:
  %a = alloca i32
  %condInit = alloca i1
  store i32 0, ptr %a
  store i1 1, ptr %condInit  ; Condition variable initially true

  br label %loopHead

loopHead:
  %condVal = load i1, ptr %condInit
  br i1 %condVal, label %loopBody, label %loopEnd

loopBody:
  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %a
  ; End the loop
  store i1 0, ptr %condInit
  br label %loopHead

loopEnd:
  %finalVal = load i32, ptr %a
  call void @SINK(i32 %finalVal)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
