
define i32 @main() {
  %x = alloca i32
  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %x
  store i32 42, ptr %x        ; Overwrite with a non-secret value
  %nonSecret = load i32, ptr %x
  call void @SINK(i32 %nonSecret)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
