
define i32 @main() {
  %x = alloca i32
  store i32 5, ptr %x
  %val = load i32, ptr %x
  call void @SINK(i32 %val)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
