
define i32 @main() {
  %a = alloca i32
  %secret1 = call i32 () @SOURCE()
  store i32 %secret1, ptr %a
  %secret2 = call i32 () @SOURCE()
  store i32 %secret2, ptr %a  ; overwrite with another secret
  store i32 99, ptr %a        ; now overwrite with non-secret
  %finalVal = load i32, ptr %a
  call void @SINK(i32 %finalVal)
  ret i32 0
}

declare i32 @SOURCE()
declare void @SINK(i32)
