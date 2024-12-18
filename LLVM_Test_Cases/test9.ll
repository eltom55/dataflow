
define i32 @main() {
entry:
  %a = alloca i32
  %b = alloca i32
  store i32 10, ptr %a

  %cond = icmp eq i32 10, 10
  br i1 %cond, label %branch1, label %branch2

branch1:
  %secretVal = call i32 () @SOURCE()
  store i32 %secretVal, ptr %b
  br label %merge

branch2:
  store i32 5, ptr %b
  br label %merge

merge:
  ; Overwrite %b with a safe value
  store i32 1234, ptr %b

  %mergedVal = load i32, ptr %b
  call void @SINK(i32 %mergedVal)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
