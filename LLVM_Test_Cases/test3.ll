
define i32 @main() {
  %x = alloca i32
  store i32 0, ptr %x
  %val = load i32, ptr %x
  %cond = icmp eq i32 %val, 1
  br i1 %cond, label %thenBB, label %elseBB

thenBB:
  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %x
  br label %mergeBB

elseBB:
  store i32 100, ptr %x
  br label %mergeBB

mergeBB:
  %merged = load i32, ptr %x
  call void @SINK(i32 %merged)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
