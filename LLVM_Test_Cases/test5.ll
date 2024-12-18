
define i32 @main() {
  %x = alloca i32
  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %x
  ; No SINK call here
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
