
define i32 @main() {
  %x = alloca i32
  %y = alloca i32
  store i32 0, ptr %x
  store i32 0, ptr %y

  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %y ; y is tainted now

  %xVal = load i32, ptr %x     ; non-secret
  call void @SINK(i32 %xVal)

  %yVal = load i32, ptr %y     ; secret
  call void @SINK(i32 %yVal)

  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
