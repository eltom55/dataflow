
define i32 @main() {
  %a = alloca i32
  %b = alloca i32
  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %a
  store i32 %secretVal, ptr %b

  ; Overwrite only %a, but not %b
  store i32 0, ptr %a

  %aVal = load i32, ptr %a
  %bVal = load i32, ptr %b

  ; Pass %bVal (still secret) to SINK
  call void @SINK(i32 %bVal)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
